//*@@@+++@@@@******************************************************************
//
// Copyright � Microsoft Corp.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// � Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// � Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*@@@---@@@@******************************************************************

/* Modified by Josh Aas, Mozilla Corportion */

/* g++ -I../../jxrlib/jxrtestlib -I../../jxrlib/common/include -I../../jxrlib/jxrgluelib -I../../jxrlib/image/sys -D__ANSI__ -o yuvjxr -L../../jxrlib -ljpegxr -ljxrglue yuvjxr.c */

#define _CRT_SECURE_NO_WARNINGS
#include <JXRTest.h>
#include <errno.h>

// optimized for PSNR
const int QP_TAB_SIZE = 11;

int DPK_QPS_420[11][6] = {      // for 8 bit only
    { 66, 65, 70, 72, 72, 77 },
    { 59, 58, 63, 64, 63, 68 },
    { 52, 51, 57, 56, 56, 61 },
    { 48, 48, 54, 51, 50, 55 },
    { 43, 44, 48, 46, 46, 49 },
    { 37, 37, 42, 38, 38, 43 },
    { 26, 28, 31, 27, 28, 31 },
    { 16, 17, 22, 16, 17, 21 },
    { 10, 11, 13, 10, 10, 13 },
    {  5,  5,  6,  5,  5,  6 },
    {  2,  2,  3,  2,  2,  2 }
};

void init_encoder_params(CWMIStrCodecParam& params, int quality_i)
{
    memset(&params, 0, sizeof(params));

    params.bYUVData = true;
    params.cfColorFormat = YUV_420;
    params.bdBitDepth    = BD_LONG;

    // quality 100 means lossless - which can be flagged by qp indecies of '0'
    if( quality_i < 100 )
    {
        // convert quality parameter into QP indecies
        float quality = ((float)quality_i) / 100.f;
        int   index   = (int) ((QP_TAB_SIZE-1) * quality); 
        float frac    = ((QP_TAB_SIZE-1) * quality) - (float)index;
    
        const int *pQPs = DPK_QPS_420[index];
    
        params.uiDefaultQPIndex    = 
            (U8) (0.5f + (float) pQPs[0] * (1.f - frac) + (float) (pQPs + 6)[0] * frac);
        params.uiDefaultQPIndexU   = 
            (U8) (0.5f + (float) pQPs[1] * (1.f - frac) + (float) (pQPs + 6)[1] * frac);
        params.uiDefaultQPIndexV   = 
            (U8) (0.5f + (float) pQPs[2] * (1.f - frac) + (float) (pQPs + 6)[2] * frac);
        params.uiDefaultQPIndexYHP = 
            (U8) (0.5f + (float) pQPs[3] * (1.f - frac) + (float) (pQPs + 6)[3] * frac);
        params.uiDefaultQPIndexUHP = 
            (U8) (0.5f + (float) pQPs[4] * (1.f - frac) + (float) (pQPs + 6)[4] * frac);
        params.uiDefaultQPIndexVHP = 
            (U8) (0.5f + (float) pQPs[5] * (1.f - frac) + (float) (pQPs + 6)[5] * frac);
    }
}

void
pack_yuv(int* image_buffer, unsigned char* source_buffer, int width, int height)
{
    unsigned char* yps0 = source_buffer;
    unsigned char* yps1 = source_buffer+width;
    unsigned char* ups  = source_buffer+width*height;
    unsigned char* vps  = ups + (width>>1)*(height>>1);

    int* dp = image_buffer;

    // JXR encoder expects Y00 Y10 Y01 Y11 U0 V0 packed and as signed integers
	int x, y;
    for( y = 0; y < height; y+=2 ) {
        for( x = 0; x < width; x+=2, dp+=6, yps0+=2, yps1+=2, ups++, vps++) {
            dp[0] = ((int)*(yps0+0) - 128) << 3;
            dp[1] = ((int)*(yps0+1) - 128) << 3;
            dp[2] = ((int)*(yps1+0) - 128) << 3;
            dp[3] = ((int)*(yps1+1) - 128) << 3;
            dp[4] = ((int)*ups - 128) << 3;
            dp[5] = ((int)*vps - 128) << 3;
        }
        yps0 += width;
        yps1 += width;
    }
}

//================================================================
// main function
//================================================================
int 
#ifndef __ANSI__
__cdecl 
#endif // __ANSI__
main(int argc, char* argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Required arguments:\n");
        fprintf(stderr, "1. JPEG quality value, 0-100\n");
        fprintf(stderr, "2. Image size (e.g. '512x512')\n");
        fprintf(stderr, "3. Path to YUV input file\n");
        fprintf(stderr, "4. Path to JXR output file\n");
        return 1;
    }

    errno = 0;

    long quality = strtol(argv[1], NULL, 10);
    if (errno != 0 || quality < 0 || quality > 100) {
        fprintf(stderr, "Invalid JPEG quality value!\n");
        return 1;
    }

    const char *size = argv[2];
    const char *x = strchr(size, 'x');
    if (!x && x != size && x != (x + strlen(x) - 1)) {
        fprintf(stderr, "Invalid image size input!\n");
        return 1;
    }
    long width = strtol(size, NULL, 10);
    if (errno != 0) {
        fprintf(stderr, "Invalid image size input!\n");
        return 1;
    }
    long height = strtol(x + 1, NULL, 10);
    if (errno != 0) {
        fprintf(stderr, "Invalid image size input!\n");
        return 1;
    }
    /* Right now we only support dimensions that are multiples of 16. */
    if ((width % 16) != 0 || (height % 16) != 0) {
        fprintf(stderr, "Image dimensions must be multiples of 16!\n");
        return 1;
    }

    /* Will check these for validity when opening via 'fopen'. */
    const char *yuv_path = argv[3];
    const char *jxr_path = argv[4];

    FILE *yuv_fd = fopen(yuv_path, "r");
    if (!yuv_fd) {
        fprintf(stderr, "Invalid path to YUV file!\n");
        return 1;
    }

    fseek(yuv_fd, 0, SEEK_END);
    long yuv_size = ftell(yuv_fd);
    fseek(yuv_fd, 0, SEEK_SET);

    unsigned char *source_buffer = (unsigned char*)malloc(yuv_size);
    fread(source_buffer, yuv_size, 1, yuv_fd);

    fclose(yuv_fd);

    // JXR encoder expects packed YUV
    int *image_buffer = (int*)malloc(yuv_size*4);
    pack_yuv(image_buffer, source_buffer, width, height);
    free(source_buffer);

    // set encoder parameters including quality
    {
        CWMIStrCodecParam params;
        init_encoder_params(params, quality);
    
        // run encoder
        ERR err;
        PKFactory*        pFactory      = NULL;
        PKCodecFactory*   pCodecFactory = NULL;
        struct WMPStream* pEncodeStream = NULL;
        PKImageEncode*    pEncoder      = NULL;
    
        Call( PKCreateFactory(&pFactory, PK_SDK_VERSION) );
        Call( pFactory->CreateStreamFromFilename(&pEncodeStream, jxr_path, "wb") );
    
        Call( PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION) );
        Call( pCodecFactory->CreateCodec(&IID_PKImageWmpEncode, (void**)&pEncoder) );
        Call( pEncoder->Initialize(pEncoder, pEncodeStream, &params, sizeof(params)) );
        Call( pEncoder->SetPixelFormat(pEncoder, GUID_PKPixelFormat12bppYCC420) );
        Call( pEncoder->SetSize(pEncoder, width, height) );
        Call( pEncoder->WritePixels(pEncoder, height, (U8*)image_buffer, width*3*4) ); 

Cleanup:
         if( pEncoder )      pEncoder->Release(&pEncoder);
         if( pCodecFactory ) pCodecFactory->Release(&pCodecFactory);
         if( pFactory )      pFactory->Release(&pFactory);
    }

    free(image_buffer);

    return 0;
}
