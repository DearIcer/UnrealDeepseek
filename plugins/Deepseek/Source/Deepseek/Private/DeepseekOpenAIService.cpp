#include "DeepseekOpenAIService.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

FDeepseekOpenAIService::FDeepseekOpenAIService()
    : HttpModule(nullptr)
{
    HttpModule = &FHttpModule::Get();
}

FDeepseekOpenAIService::~FDeepseekOpenAIService()
{
}

void FDeepseekOpenAIService::Initialize(const FString& InApiKey, const FString& InModel, const FString& InApiUrl)
{
    ApiKey = InApiKey;
    Model = InModel;
    ApiUrl = InApiUrl;
}

void FDeepseekOpenAIService::SendChatRequest(const TArray<FOpenAIMessage>& Messages, TFunction<void(const FString&, bool)> OnCompleted)
{
    if (ApiKey.IsEmpty())
    {
        OnCompleted(TEXT("API密钥未设置"), false);
        return;
    }

    if (ApiUrl.IsEmpty())
    {
        OnCompleted(TEXT("API地址未设置"), false);
        return;
    }

    // 创建HTTP请求
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetURL(ApiUrl);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));

    // 创建请求体
    TSharedPtr<FJsonObject> RequestObj = MakeShared<FJsonObject>();
    RequestObj->SetStringField(TEXT("model"), Model);
    
    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FOpenAIMessage& Message : Messages)
    {
        TSharedPtr<FJsonObject> MessageObj = MakeShared<FJsonObject>();
        MessageObj->SetStringField(TEXT("role"), Message.Role);
        MessageObj->SetStringField(TEXT("content"), Message.Content);
        MessagesArray.Add(MakeShared<FJsonValueObject>(MessageObj));
    }
    RequestObj->SetArrayField(TEXT("messages"), MessagesArray);
    
    // 可选参数
    RequestObj->SetNumberField(TEXT("temperature"), 0.7);
    RequestObj->SetNumberField(TEXT("max_tokens"), 1000);
    RequestObj->SetBoolField(TEXT("stream"), false);

    // 将JSON对象转换为字符串
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);

    // 设置请求体
    HttpRequest->SetContentAsString(RequestBody);

    // 创建回调函数
    TSharedPtr<TFunction<void(const FString&, bool)>> SharedOnCompleted = MakeShared<TFunction<void(const FString&, bool)>>(MoveTemp(OnCompleted));

    // 设置回调
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, SharedOnCompleted](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            this->HandleResponse(Response, bWasSuccessful, *SharedOnCompleted);
        }
    );

    // 发送请求
    HttpRequest->ProcessRequest();
}

void FDeepseekOpenAIService::HandleResponse(FHttpResponsePtr Response, bool bWasSuccessful, TFunction<void(const FString&, bool)> OnCompleted)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        OnCompleted(TEXT("请求失败"), false);
        return;
    }

    FString ResponseString = Response->GetContentAsString();
    
    if (Response->GetResponseCode() != 200)
    {
        OnCompleted(FString::Printf(TEXT("API错误: %d\n%s"), Response->GetResponseCode(), *ResponseString), false);
        return;
    }

    FOpenAIResponse OpenAIResponse;
    if (!ParseResponse(ResponseString, OpenAIResponse))
    {
        OnCompleted(TEXT("解析响应失败"), false);
        return;
    }

    if (OpenAIResponse.Choices.Num() > 0)
    {
        OnCompleted(OpenAIResponse.Choices[0].Message.Content, true);
    }
    else
    {
        OnCompleted(TEXT("没有收到回复"), false);
    }
}

bool FDeepseekOpenAIService::ParseResponse(const FString& ResponseString, FOpenAIResponse& OutResponse)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    // 解析基本信息
    OutResponse.Id = JsonObject->GetStringField(TEXT("id"));
    OutResponse.Object = JsonObject->GetStringField(TEXT("object"));
    OutResponse.Created = JsonObject->GetIntegerField(TEXT("created"));
    OutResponse.Model = JsonObject->GetStringField(TEXT("model"));

    // 解析选择
    TArray<TSharedPtr<FJsonValue>> ChoicesArray = JsonObject->GetArrayField(TEXT("choices"));
    for (const TSharedPtr<FJsonValue>& ChoiceValue : ChoicesArray)
    {
        TSharedPtr<FJsonObject> ChoiceObject = ChoiceValue->AsObject();
        FOpenAIChoice Choice;
        
        Choice.Index = ChoiceObject->GetIntegerField(TEXT("index"));
        Choice.FinishReason = ChoiceObject->GetStringField(TEXT("finish_reason"));
        
        TSharedPtr<FJsonObject> MessageObject = ChoiceObject->GetObjectField(TEXT("message"));
        Choice.Message.Role = MessageObject->GetStringField(TEXT("role"));
        Choice.Message.Content = MessageObject->GetStringField(TEXT("content"));
        
        OutResponse.Choices.Add(Choice);
    }

    // 解析使用情况
    TSharedPtr<FJsonObject> UsageObject = JsonObject->GetObjectField(TEXT("usage"));
    OutResponse.Usage.PromptTokens = UsageObject->GetIntegerField(TEXT("prompt_tokens"));
    OutResponse.Usage.CompletionTokens = UsageObject->GetIntegerField(TEXT("completion_tokens"));
    OutResponse.Usage.TotalTokens = UsageObject->GetIntegerField(TEXT("total_tokens"));

    return true;
} 