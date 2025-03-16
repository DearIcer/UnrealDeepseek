#include "SDeepseekAIChat.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/SlateTypes.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDeepseekAIChat::Construct(const FArguments& InArgs)
{
    // 初始化变量
    bIsWaiting = false;
    WaitingMessageIndex = -1;
    bShowSettings = false;

    // 初始化当前设置
    CurrentApiKey = InArgs._ApiKey;
    CurrentApiUrl = InArgs._ApiUrl;
    CurrentModel = InArgs._Model;

    // 初始化模型列表
    ModelList.Add(MakeShared<FModelInfo>(TEXT("deepseek-chat"), TEXT("deepseek-chat")));
    ModelList.Add(MakeShared<FModelInfo>(TEXT("deepseek-reasoner"), TEXT("deepseek-reasoner")));
    
    // 设置当前选择的模型
    for (TSharedPtr<FModelInfo> ModelInfo : ModelList)
    {
        if (ModelInfo->Id == CurrentModel)
        {
            SelectedModel = ModelInfo;
            break;
        }
    }
    
    if (!SelectedModel.IsValid() && ModelList.Num() > 0)
    {
        SelectedModel = ModelList[0];
        CurrentModel = SelectedModel->Id;
    }

    // 创建OpenAI服务
    OpenAIService = MakeShared<FDeepseekOpenAIService>();
    OpenAIService->Initialize(CurrentApiKey, CurrentModel, CurrentApiUrl);

    // 创建聊天列表视图
    ChatListView = SNew(SListView<TSharedPtr<FChatMessage>>)
        .ListItemsSource(&ChatMessages)
        .OnGenerateRow(this, &SDeepseekAIChat::OnGenerateRow)
        .SelectionMode(ESelectionMode::None);

    // 添加欢迎消息
    ChatMessages.Add(MakeShared<FChatMessage>(TEXT("AI助手"), TEXT("您好！我是Deepseek AI助手，请问有什么可以帮助您的？"), false));
    
    // 添加系统消息到聊天历史
    ChatHistory.Add(FOpenAIMessage(TEXT("system"), TEXT("你是一个有用的AI助手，由Deepseek团队开发。请用中文回答问题，保持回答简洁明了。")));

    // 创建设置面板
    SettingsPanel = SNew(SBorder)
        .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(FMargin(8.0f))
        [
            SNew(SVerticalBox)
            
            // 标题
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("设置")))
                .Font(FEditorStyle::GetFontStyle("HeadingExtraLarge"))
                .Justification(ETextJustify::Center)
            ]
            
            // API密钥
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(SVerticalBox)
                
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 5)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("API密钥:")))
                    .Font(FEditorStyle::GetFontStyle("BoldFont"))
                ]
                
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(ApiKeyTextBox, SEditableTextBox)
                    .Text(FText::FromString(CurrentApiKey))
                    .HintText(FText::FromString(TEXT("请输入您的API密钥...")))
                ]
            ]
            
            // API地址
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(SVerticalBox)
                
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 5)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("API地址:")))
                    .Font(FEditorStyle::GetFontStyle("BoldFont"))
                ]
                
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(ApiUrlTextBox, SEditableTextBox)
                    .Text(FText::FromString(CurrentApiUrl))
                    .HintText(FText::FromString(TEXT("请输入API地址...")))
                ]
            ]
            
            // 模型选择
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(SVerticalBox)
                
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 5)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("模型:")))
                    .Font(FEditorStyle::GetFontStyle("BoldFont"))
                ]
                
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(ModelComboBox, SComboBox<TSharedPtr<FModelInfo>>)
                    .OptionsSource(&ModelList)
                    .OnSelectionChanged(this, &SDeepseekAIChat::OnModelSelectionChanged)
                    .OnGenerateWidget(this, &SDeepseekAIChat::GenerateModelComboItem)
                    .InitiallySelectedItem(SelectedModel)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() {
                            return SelectedModel.IsValid() ? FText::FromString(SelectedModel->Name) : FText::FromString(TEXT("请选择模型"));
                        })
                    ]
                ]
            ]
            
            // 按钮
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 10, 0, 0)
            [
                SNew(SHorizontalBox)
                
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0, 0, 5, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("应用")))
                    .OnClicked(this, &SDeepseekAIChat::OnApplySettings)
                ]
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("取消")))
                    .OnClicked(this, &SDeepseekAIChat::OnHideSettings)
                ]
            ]
        ];

    ChildSlot
    [
        SNew(SVerticalBox)
        
        // 主界面
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBorder)
            .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(FMargin(4.0f))
            [
                SNew(SVerticalBox)
                
                // 标题和设置按钮
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 10)
                [
                    SNew(SHorizontalBox)
                    
                    // 标题
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Deepseek AI 问答助手")))
                        .Font(FEditorStyle::GetFontStyle("HeadingExtraLarge"))
                        .Justification(ETextJustify::Center)
                    ]
                    
                    // 设置按钮
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("设置")))
                        .OnClicked(this, &SDeepseekAIChat::OnShowSettings)
                    ]
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
        ]
        
        // 设置面板
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(0, 10, 0, 0)
        [
            SNew(SWidgetSwitcher)
            .WidgetIndex_Lambda([this]() { return bShowSettings ? 0 : 1; })
            
            + SWidgetSwitcher::Slot()
            [
                SettingsPanel.ToSharedRef()
            ]
            
            + SWidgetSwitcher::Slot()
            [
                SNullWidget::NullWidget
            ]
        ]
    ];
}

FReply SDeepseekAIChat::OnSendMessage()
{
    if (bIsWaiting || bShowSettings)
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
    if (bIsWaiting || bShowSettings)
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

FReply SDeepseekAIChat::OnShowSettings()
{
    if (!bIsWaiting)
    {
        bShowSettings = true;
        
        // 更新设置面板的值
        ApiKeyTextBox->SetText(FText::FromString(CurrentApiKey));
        ApiUrlTextBox->SetText(FText::FromString(CurrentApiUrl));
        
        // 更新模型选择
        for (TSharedPtr<FModelInfo> ModelInfo : ModelList)
        {
            if (ModelInfo->Id == CurrentModel)
            {
                SelectedModel = ModelInfo;
                ModelComboBox->SetSelectedItem(SelectedModel);
                break;
            }
        }
    }
    
    return FReply::Handled();
}

FReply SDeepseekAIChat::OnHideSettings()
{
    bShowSettings = false;
    return FReply::Handled();
}

FReply SDeepseekAIChat::OnApplySettings()
{
    // 获取新的设置
    FString NewApiKey = ApiKeyTextBox->GetText().ToString();
    FString NewApiUrl = ApiUrlTextBox->GetText().ToString();
    FString NewModel = SelectedModel.IsValid() ? SelectedModel->Id : TEXT("gpt-3.5-turbo");
    
    // 检查是否有变化
    bool bHasChanges = (NewApiKey != CurrentApiKey) || (NewApiUrl != CurrentApiUrl) || (NewModel != CurrentModel);
    
    if (bHasChanges)
    {
        // 更新当前设置
        CurrentApiKey = NewApiKey;
        CurrentApiUrl = NewApiUrl;
        CurrentModel = NewModel;
        
        // 重新初始化OpenAI服务
        OpenAIService->Initialize(CurrentApiKey, CurrentModel, CurrentApiUrl);
        
        // 添加系统消息
        ChatMessages.Add(MakeShared<FChatMessage>(TEXT("系统"), TEXT("设置已更新"), false));
        ChatListView->RequestListRefresh();
    }
    
    // 隐藏设置面板
    bShowSettings = false;
    
    return FReply::Handled();
}

void SDeepseekAIChat::OnModelSelectionChanged(TSharedPtr<FModelInfo> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        SelectedModel = NewSelection;
    }
}

TSharedRef<SWidget> SDeepseekAIChat::GenerateModelComboItem(TSharedPtr<FModelInfo> InItem)
{
    return SNew(STextBlock)
        .Text(FText::FromString(InItem->Name));
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