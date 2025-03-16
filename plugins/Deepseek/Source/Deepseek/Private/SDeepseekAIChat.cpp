#include "SDeepseekAIChat.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/SlateTypes.h"
#include "EditorStyleSet.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDeepseekAIChat::Construct(const FArguments& InArgs)
{
    // 初始化变量
    bIsWaiting = false;
    WaitingMessageIndex = -1;

    // 创建OpenAI服务
    OpenAIService = MakeShared<FDeepseekOpenAIService>();
    OpenAIService->Initialize(InArgs._ApiKey, InArgs._Model);

    // 创建聊天列表视图
    ChatListView = SNew(SListView<TSharedPtr<FChatMessage>>)
        .ListItemsSource(&ChatMessages)
        .OnGenerateRow(this, &SDeepseekAIChat::OnGenerateRow)
        .SelectionMode(ESelectionMode::None);

    // 添加欢迎消息
    ChatMessages.Add(MakeShared<FChatMessage>(TEXT("AI助手"), TEXT("您好！我是Deepseek AI助手，请问有什么可以帮助您的？"), false));
    
    // 添加系统消息到聊天历史
    ChatHistory.Add(FOpenAIMessage(TEXT("system"), TEXT("你是一个有用的AI助手，由Deepseek团队开发。请用中文回答问题，保持回答简洁明了。")));

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(4.0f))
        [
            SNew(SVerticalBox)
            
            // 标题
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Deepseek AI 问答助手")))
                .Font(FEditorStyle::GetFontStyle("HeadingExtraLarge"))
                .Justification(ETextJustify::Center)
            ]
            
            // 聊天记录区域
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0, 0, 0, 10)
            [
                SNew(SBorder)
                .BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
                .Padding(FMargin(4.0f))
                [
                    SNew(SScrollBox)
                    .ScrollBarAlwaysVisible(true)
                    + SScrollBox::Slot()
                    [
                        ChatListView.ToSharedRef()
                    ]
                ]
            ]
            
            // 输入区域
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                
                // 输入框
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0, 0, 5, 0)
                [
                    SAssignNew(InputTextBox, SEditableTextBox)
                    .HintText(FText::FromString(TEXT("请输入您的问题...")))
                    .IsEnabled(!bIsWaiting)
                    .OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
                    {
                        if (CommitType == ETextCommit::OnEnter && !Text.IsEmpty() && !bIsWaiting)
                        {
                            OnSendMessage();
                        }
                    })
                ]
                
                // 发送按钮
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0, 0, 5, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("发送")))
                    .IsEnabled(!bIsWaiting)
                    .OnClicked(this, &SDeepseekAIChat::OnSendMessage)
                ]
                
                // 清空按钮
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("清空")))
                    .IsEnabled(!bIsWaiting)
                    .OnClicked(this, &SDeepseekAIChat::OnClearChat)
                ]
            ]
        ]
    ];
}

FReply SDeepseekAIChat::OnSendMessage()
{
    if (bIsWaiting)
    {
        return FReply::Handled();
    }

    FString UserMessage = InputTextBox->GetText().ToString();
    
    if (!UserMessage.IsEmpty())
    {
        // 添加用户消息
        ChatMessages.Add(MakeShared<FChatMessage>(TEXT("用户"), UserMessage, true));
        
        // 清空输入框
        InputTextBox->SetText(FText::GetEmpty());
        
        // 刷新列表
        ChatListView->RequestListRefresh();
        
        // 发送AI请求
        SendAIRequest(UserMessage);
    }
    
    return FReply::Handled();
}

FReply SDeepseekAIChat::OnClearChat()
{
    if (bIsWaiting)
    {
        return FReply::Handled();
    }

    // 清空聊天记录，保留欢迎消息
    ChatMessages.Empty();
    ChatMessages.Add(MakeShared<FChatMessage>(TEXT("AI助手"), TEXT("您好！我是Deepseek AI助手，请问有什么可以帮助您的？"), false));
    
    // 清空聊天历史，保留系统消息
    ChatHistory.Empty();
    ChatHistory.Add(FOpenAIMessage(TEXT("system"), TEXT("你是一个有用的AI助手，由Deepseek团队开发。请用中文回答问题，保持回答简洁明了。")));
    
    // 刷新列表
    ChatListView->RequestListRefresh();
    
    return FReply::Handled();
}

void SDeepseekAIChat::SendAIRequest(const FString& UserMessage)
{
    // 添加用户消息到聊天历史
    ChatHistory.Add(FOpenAIMessage(TEXT("user"), UserMessage));
    
    // 添加等待消息
    AddWaitingMessage();
    
    // 发送请求
    OpenAIService->SendChatRequest(ChatHistory, [this](const FString& Response, bool bSuccess)
    {
        // 在游戏线程中处理响应
        AsyncTask(ENamedThreads::GameThread, [this, Response, bSuccess]()
        {
            HandleAIResponse(Response, bSuccess);
        });
    });
}

void SDeepseekAIChat::HandleAIResponse(const FString& Response, bool bSuccess)
{
    // 移除等待消息
    RemoveWaitingMessage();
    
    if (bSuccess)
    {
        // 添加AI回复
        ChatMessages.Add(MakeShared<FChatMessage>(TEXT("AI助手"), Response, false));
        
        // 添加AI回复到聊天历史
        ChatHistory.Add(FOpenAIMessage(TEXT("assistant"), Response));
    }
    else
    {
        // 添加错误消息
        ChatMessages.Add(MakeShared<FChatMessage>(TEXT("系统"), FString::Printf(TEXT("错误: %s"), *Response), false));
    }
    
    // 刷新列表
    ChatListView->RequestListRefresh();
}

void SDeepseekAIChat::AddWaitingMessage()
{
    bIsWaiting = true;
    
    // 添加等待消息
    TSharedPtr<FChatMessage> WaitingMessage = MakeShared<FChatMessage>(TEXT("AI助手"), TEXT("正在思考..."), false);
    ChatMessages.Add(WaitingMessage);
    WaitingMessageIndex = ChatMessages.Num() - 1;
    
    // 刷新列表
    ChatListView->RequestListRefresh();
}

void SDeepseekAIChat::RemoveWaitingMessage()
{
    if (WaitingMessageIndex >= 0 && WaitingMessageIndex < ChatMessages.Num())
    {
        ChatMessages.RemoveAt(WaitingMessageIndex);
        WaitingMessageIndex = -1;
    }
    
    bIsWaiting = false;
}

TSharedRef<ITableRow> SDeepseekAIChat::OnGenerateRow(TSharedPtr<FChatMessage> Message, const TSharedRef<STableViewBase>& OwnerTable)
{
    // 根据消息发送者设置不同的样式
    const FSlateBrush* BubbleBrush = Message->bIsUser 
        ? FEditorStyle::GetBrush("ToolPanel.GroupBorder") 
        : FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder");
    
    EHorizontalAlignment HAlign = Message->bIsUser ? HAlign_Right : HAlign_Left;
    
    return SNew(STableRow<TSharedPtr<FChatMessage>>, OwnerTable)
        .Padding(FMargin(4.0f))
        .ShowSelection(false)
        [
            SNew(SHorizontalBox)
            
            // 用户消息靠右，AI消息靠左
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .HAlign(HAlign)
            [
                SNew(SBox)
                .MaxDesiredWidth(500.0f)
                [
                    SNew(SBorder)
                    .BorderImage(BubbleBrush)
                    .Padding(FMargin(10.0f))
                    [
                        SNew(SVerticalBox)
                        
                        // 发送者名称
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 5)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Message->Sender))
                            .Font(FEditorStyle::GetFontStyle("BoldFont"))
                        ]
                        
                        // 消息内容
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Message->Message))
                            .AutoWrapText(true)
                        ]
                    ]
                ]
            ]
        ];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION 