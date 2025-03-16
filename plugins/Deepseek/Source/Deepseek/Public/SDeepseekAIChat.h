#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "DeepseekOpenAIService.h"

/**
 * 聊天消息结构体
 */
struct FChatMessage
{
    FString Sender;
    FString Message;
    bool bIsUser;

    FChatMessage(const FString& InSender, const FString& InMessage, bool bInIsUser)
        : Sender(InSender), Message(InMessage), bIsUser(bInIsUser)
    {}
};

/**
 * 模型信息结构体
 */
struct FModelInfo
{
    FString Id;
    FString Name;

    FModelInfo(const FString& InId, const FString& InName)
        : Id(InId), Name(InName)
    {}
};

/**
 * AI问答界面小部件
 */
class DEEPSEEK_API SDeepseekAIChat : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDeepseekAIChat)
        : _ApiKey("")
        , _ApiUrl("https://api.openai.com/v1/chat/completions")
        , _Model("gpt-3.5-turbo")
    {}
        SLATE_ARGUMENT(FString, ApiKey)
        SLATE_ARGUMENT(FString, ApiUrl)
        SLATE_ARGUMENT(FString, Model)
    SLATE_END_ARGS()

    /** 构造函数 */
    void Construct(const FArguments& InArgs);

private:
    /** 发送消息回调 */
    FReply OnSendMessage();
    
    /** 清空聊天记录回调 */
    FReply OnClearChat();
    
    /** 发送AI请求 */
    void SendAIRequest(const FString& UserMessage);
    
    /** 处理AI响应 */
    void HandleAIResponse(const FString& Response, bool bSuccess);
    
    /** 创建聊天消息行 */
    TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FChatMessage> Message, const TSharedRef<STableViewBase>& OwnerTable);

    /** 添加等待消息 */
    void AddWaitingMessage();

    /** 移除等待消息 */
    void RemoveWaitingMessage();

    /** 显示设置面板 */
    FReply OnShowSettings();

    /** 隐藏设置面板 */
    FReply OnHideSettings();

    /** 应用设置 */
    FReply OnApplySettings();

    /** 模型选择改变回调 */
    void OnModelSelectionChanged(TSharedPtr<FModelInfo> NewSelection, ESelectInfo::Type SelectInfo);

    /** 生成模型选择项 */
    TSharedRef<SWidget> GenerateModelComboItem(TSharedPtr<FModelInfo> InItem);

private:
    /** 聊天消息列表 */
    TArray<TSharedPtr<FChatMessage>> ChatMessages;
    
    /** 聊天消息列表视图 */
    TSharedPtr<SListView<TSharedPtr<FChatMessage>>> ChatListView;
    
    /** 输入文本框 */
    TSharedPtr<SEditableTextBox> InputTextBox;

    /** OpenAI服务 */
    TSharedPtr<FDeepseekOpenAIService> OpenAIService;

    /** 聊天历史 */
    TArray<FOpenAIMessage> ChatHistory;

    /** 是否正在等待AI响应 */
    bool bIsWaiting;

    /** 等待消息的索引 */
    int32 WaitingMessageIndex;

    /** 设置面板 */
    TSharedPtr<SWidget> SettingsPanel;

    /** 是否显示设置面板 */
    bool bShowSettings;

    /** API密钥输入框 */
    TSharedPtr<SEditableTextBox> ApiKeyTextBox;

    /** API地址输入框 */
    TSharedPtr<SEditableTextBox> ApiUrlTextBox;

    /** 模型选择下拉框 */
    TSharedPtr<SComboBox<TSharedPtr<FModelInfo>>> ModelComboBox;

    /** 模型列表 */
    TArray<TSharedPtr<FModelInfo>> ModelList;

    /** 当前选择的模型 */
    TSharedPtr<FModelInfo> SelectedModel;

    /** 当前API密钥 */
    FString CurrentApiKey;

    /** 当前API地址 */
    FString CurrentApiUrl;

    /** 当前模型 */
    FString CurrentModel;
}; 