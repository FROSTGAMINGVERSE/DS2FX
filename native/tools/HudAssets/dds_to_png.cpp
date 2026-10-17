#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void need(HRESULT result,const char* reason){if(FAILED(result))throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv){try {
    if(argc!=3)throw std::runtime_error("Usage: DS2FX-IconConverter <DDS> <new PNG>");
    if(GetFileAttributesW(argv[2])!=INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Output already exists");
    need(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM initialization failed");
    ComPtr<IWICImagingFactory> factory;need(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"WIC factory failed");
    ComPtr<IWICBitmapDecoder> decoder;need(factory->CreateDecoderFromFilename(argv[1],nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder),"Windows DDS decoding failed");
    ComPtr<IWICBitmapFrameDecode> frame;need(decoder->GetFrame(0,&frame),"DDS frame failed");UINT width=0,height=0;frame->GetSize(&width,&height);
    if(width!=height || (width!=32 && width!=64))throw std::runtime_error("Unsupported icon size");
    ComPtr<IWICFormatConverter> converter;need(factory->CreateFormatConverter(&converter),"Converter failed");
    need(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"RGBA conversion failed");
    ComPtr<IWICStream> stream;factory->CreateStream(&stream);need(stream->InitializeFromFilename(argv[2],GENERIC_WRITE),"PNG stream failed");
    ComPtr<IWICBitmapEncoder> encoder;need(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"PNG encoder failed");encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);
    ComPtr<IWICBitmapFrameEncode> output;encoder->CreateNewFrame(&output,nullptr);output->Initialize(nullptr);output->SetSize(width,height);
    auto format=GUID_WICPixelFormat32bppBGRA;output->SetPixelFormat(&format);need(output->WriteSource(converter.Get(),nullptr),"PNG write failed");need(output->Commit(),"PNG frame commit failed");need(encoder->Commit(),"PNG commit failed");return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
