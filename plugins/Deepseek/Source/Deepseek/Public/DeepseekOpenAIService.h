#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Json.h"
#include "JsonObjectConverter.h"

/**
 * OpenAI API响应的消息结构
 */
struct FOpenAIMessage
{
	FString Role;
	FString Content;

	FOpenAIMessage() {}
	FOpenAIMessage(const FString& InRole, const FString& InContent) : Role(InRole), Content(InContent) {}
};

/**
 * OpenAI API响应的选择结构
 */
struct FOpenAIChoice
{
	FOpenAIMessage Message;
	FString FinishReason;
	int32 Index;
};

/**
 * OpenAI API响应的使用情况结构
 */
struct FOpenAIUsage
{
	int32 PromptTokens;
	int32 CompletionTokens;
	int32 TotalTokens;
};

/**
 * OpenAI API响应结构
 */
struct FOpenAIResponse
{
	FString Id;
	FString Object;
	int32 Created;
	FString Model;
	TArray<FOpenAIChoice> Choices;
	FOpenAIUsage Usage;
};

/**
 * OpenAI服务类，用于与OpenAI API通信
 */
class DEEPSEEK_API FDeepseekOpenAIService
{
public:
	/** 构造函数 */
	FDeepseekOpenAIService();

	/** 析构函数 */
	~FDeepseekOpenAIService();

	/** 初始化服务 */
	void Initialize(const FString& InApiKey, const FString& InModel = TEXT("deepseek-chat"), const FString& InApiUrl = TEXT("https://api.deepseek.com/chat/completions"));

	/** 发送聊天请求 */
	void SendChatRequest(const TArray<FOpenAIMessage>& Messages, TFunction<void(const FString&, bool)> OnCompleted);

private:
	/** 处理HTTP响应 */
	void HandleResponse(FHttpResponsePtr Response, bool bWasSuccessful, TFunction<void(const FString&, bool)> OnCompleted);

	/** 解析JSON响应 */
	bool ParseResponse(const FString& ResponseString, FOpenAIResponse& OutResponse);

private:
	/** API密钥 */
	FString ApiKey;

	/** 模型名称 */
	FString Model;

	/** API地址 */
	FString ApiUrl;

	/** HTTP请求模块 */
	FHttpModule* HttpModule;
};