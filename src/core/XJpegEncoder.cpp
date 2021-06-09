/*
    cam2web - streaming camera to web

    Copyright (C) 2017, cvsandbox, cvsandbox@gmail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "XJpegEncoder.hpp"

#include <stdio.h>
#include <jpeglib.h>

using namespace std;

namespace Private
{
    class JpegException : public exception
    {
    public:
        virtual const char* what( ) const throw( )
        {
            return "JPEG coding failure";
        }
    };

    static void my_error_exit( j_common_ptr /* cinfo */ )
    {
        throw JpegException( );
    }

    static void my_output_message( j_common_ptr /* cinfo */ )
    {
        // do nothing - kill the message
    }

    class XJpegEncoderData
    {
    public:
        uint16_t                    Quality;
        bool                        FasterCompression;
    private:
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr       jerr;

        struct jpeg_decompress_struct dcinfo;

    public:
        XJpegEncoderData( uint16_t quality, bool fasterCompression) :
            Quality( quality ), FasterCompression( fasterCompression  )
        {
            if ( Quality > 100 )
            {
                Quality = 100;
            }

            // allocate and initialize JPEG compression object
            cinfo.err           = jpeg_std_error( &jerr );
            dcinfo.err          = jpeg_std_error(&jerr);

            jerr.error_exit     = my_error_exit;
            jerr.output_message = my_output_message;

            jpeg_create_compress( &cinfo );

            jpeg_create_decompress(&dcinfo);
        }

        ~XJpegEncoderData( )
        {
            jpeg_destroy_compress( &cinfo );
            jpeg_destroy_decompress(&dcinfo);
        }

        XError EncodeToMemory( const shared_ptr<const XImage>& image, uint8_t** buffer, uint32_t* bufferSize );

        XError DecodeToMemory( const shared_ptr<const XImage>& image, cv::Mat& );

    };
}

XJpegEncoder::XJpegEncoder( uint16_t quality, bool fasterCompression ) :
    mData( new Private::XJpegEncoderData( quality, fasterCompression ) )
{

}

XJpegEncoder::~XJpegEncoder( )
{
    delete mData;
}

// Set/get compression quality, [0, 100]
uint16_t XJpegEncoder::Quality( ) const
{
    return mData->Quality;
}
void XJpegEncoder::SetQuality( uint16_t quality )
{
    mData->Quality = quality;
    if ( mData->Quality > 100 ) mData->Quality = 100;
    if ( mData->Quality < 1   ) mData->Quality = 1;
}

// Set/get faster compression (but less accurate) flag
bool XJpegEncoder::FasterCompression( ) const
{
    return mData->FasterCompression;
}
void XJpegEncoder::SetFasterCompression( bool faster )
{
    mData->FasterCompression = faster;
}

// Compress the specified image into provided buffer
XError XJpegEncoder::EncodeToMemory( const shared_ptr<const XImage>& image, uint8_t** buffer, uint32_t* bufferSize )
{
    return mData->EncodeToMemory( image, buffer, bufferSize );
}

XError XJpegEncoder::DecodeToMemory(const std::shared_ptr<const XImage>& image, cv::Mat& decMat )
{
    return mData->DecodeToMemory( image, decMat);
}


namespace Private
{

XError XJpegEncoderData::DecodeToMemory( const shared_ptr<const XImage>& image, cv::Mat& decodedMat)
{
    if ( ( !image ) || ( image->Data( ) == nullptr ) )
    {
        return XError::NullPointer;;
    }
    else if ( ( image->Format( ) != XPixelFormat::JPEG ) )
    {
        return XError::UnsupportedPixelFormat;
    }
    else
    {
        try
        {
            /* code */
            jpeg_mem_src(&dcinfo, image->Data(), image->Height()*image->Width());
            
            switch (jpeg_read_header(&dcinfo, TRUE)) {
                case JPEG_SUSPENDED:
                case JPEG_HEADER_TABLES_ONLY:
                    return XError::DamagedJPEGImage;
                case JPEG_HEADER_OK:
                break;
            }

            dcinfo.out_color_space = JCS_EXT_BGR;
            jpeg_start_decompress(&dcinfo);

            decodedMat = cv::Mat(
                            cv::Size(dcinfo.output_width, dcinfo.output_height),
                            CV_8UC3);

            while (dcinfo.output_scanline < dcinfo.output_height) 
            {
                JSAMPLE *row = decodedMat.ptr(dcinfo.output_scanline);
                jpeg_read_scanlines(&dcinfo, &row, 1);
            }            
            jpeg_finish_decompress(&dcinfo);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return XError::FailedImageEncoding;
        }
    }

    return XError::Success;
}

XError XJpegEncoderData::EncodeToMemory( const shared_ptr<const XImage>& image, uint8_t** buffer, uint32_t* bufferSize )
{
    JSAMPROW    row_pointer[1];
    XError      ret = XError::Success;

    if ( ( !image ) || ( image->Data( ) == nullptr ) || ( buffer == nullptr ) || ( *buffer == nullptr ) || ( bufferSize == nullptr ) )
    {
        ret = XError::NullPointer;
    }
    else if ( ( image->Format( ) != XPixelFormat::RGB24 ) && ( image->Format( ) != XPixelFormat::Grayscale8 ) )
    {
        ret = XError::UnsupportedPixelFormat;
    }
    else
    {
        try
        {
            // 1 - specify data destination
            unsigned long mem_buffer_size = *bufferSize;
            jpeg_mem_dest( &cinfo, buffer, &mem_buffer_size );

            // 2 - set parameters for compression
            cinfo.image_width  = image->Width( );
            cinfo.image_height = image->Height( );

            if ( image->Format( ) == XPixelFormat::RGB24 )
            {
                cinfo.input_components = 3;
                cinfo.in_color_space   = JCS_RGB;
            }
            else
            {
                cinfo.input_components = 1;
                cinfo.in_color_space   = JCS_GRAYSCALE;
            }

            // set default compression parameters
            jpeg_set_defaults( &cinfo );
            // set quality
            jpeg_set_quality( &cinfo, (int) Quality, TRUE /* limit to baseline-JPEG values */ );

            // use faster, but less accurate compressions
            cinfo.dct_method = ( FasterCompression ) ? JDCT_FASTEST : JDCT_DEFAULT;

            // 3 - start compressor
            jpeg_start_compress( &cinfo, TRUE );

            // 4 - do compression
            while ( cinfo.next_scanline < cinfo.image_height )
            {
                row_pointer[0] = image->Data( ) + image->Stride( ) * cinfo.next_scanline;

                jpeg_write_scanlines( &cinfo, row_pointer, 1 );
            }

            // 5 - finish compression
            jpeg_finish_compress( &cinfo );

            *bufferSize = (uint32_t) mem_buffer_size;
        }
        catch ( const JpegException& )
        {
            ret = XError::FailedImageEncoding;
        }
    }

    return ret;
}

} // namespace Private
