#include "GameXXKTextureAuditLibrary.h"
#include "Engine/Texture2D.h"
#include "Misc/SecureHash.h"
#include "TextureCompiler.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString UGameXXKTextureAuditLibrary::InspectTexture(UTexture2D* Texture, bool bFinishCompilation)
{
	TSharedRef<FJsonObject> Json=MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("loaded"),Texture!=nullptr);
	if(Texture)
	{
		if(bFinishCompilation) FTextureCompilingManager::Get().FinishCompilation({Texture});
		TArray64<uint8> SourcePixels;
		const bool bSourceRead=Texture->Source.GetMipData(SourcePixels,0);
		Json->SetBoolField(TEXT("source_read"),bSourceRead);
		if(bSourceRead)
		{
			FSHAHash Hash;FSHA1::HashBuffer(SourcePixels.GetData(),SourcePixels.Num(),Hash.Hash);
			Json->SetStringField(TEXT("source_pixels_sha1"),Hash.ToString());
			Json->SetNumberField(TEXT("source_width"),Texture->Source.GetSizeX());
			Json->SetNumberField(TEXT("source_height"),Texture->Source.GetSizeY());
			Json->SetNumberField(TEXT("source_format"),static_cast<int32>(Texture->Source.GetFormat()));
		}
		Json->SetNumberField(TEXT("width"),Texture->GetSizeX());
		Json->SetNumberField(TEXT("height"),Texture->GetSizeY());
		Json->SetStringField(TEXT("format"),GPixelFormats[Texture->GetPixelFormat()].Name);
		Json->SetNumberField(TEXT("resource_bytes"),Texture->CalcTextureMemorySizeEnum(TMC_ResidentMips));
		Json->SetNumberField(TEXT("payload_bytes"),Texture->CalcTextureMemorySizeEnum(TMC_AllMips));
	}
	FString Result;const auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(Json,Writer);return Result;
}
