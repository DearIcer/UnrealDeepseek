#include "SDeepseekAIChat.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/SlateTypes.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FString SDeepseekAIChat::ConfigFileName = TEXT("DeepseekAI");

void SDeepseekAIChat::Construct(const FArguments& InArgs)
{
	// 初始化变量
	bIsWaiting = false;
	WaitingMessageIndex = -1;

	// 初始化当前设置
	CurrentApiKey = InArgs._ApiKey;
	CurrentApiUrl = InArgs._ApiUrl;
	CurrentModel = InArgs._Model;
	CurrentSystemPrompt = TEXT("你是一个有用的AI助手，由Deepseek团队开发。请用中文回答问题，保持回答简洁明了。");

	// 初始化模型列表
	ModelList.Add(MakeShared<FModelInfo>(TEXT("deepseek-chat"), TEXT("deepseek-chat")));
	ModelList.Add(MakeShared<FModelInfo>(TEXT("deepseek-reasoner"), TEXT("deepseek-reasoner")));

	LoadSettings();

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
	ChatHistory.Add(FOpenAIMessage(TEXT("system"), CurrentSystemPrompt));

	ChildSlot
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
	ChatHistory.Add(FOpenAIMessage(TEXT("system"), CurrentSystemPrompt));

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
		// 创建并显示设置窗口
		TSharedRef<SWindow> SettingsWindowRef = CreateSettingsWindow();
		SettingsWindow = SettingsWindowRef;

		// 设置临时变量
		TempApiKey = CurrentApiKey;
		TempApiUrl = CurrentApiUrl;
		TempModel = CurrentModel;
		TempSystemPrompt = CurrentSystemPrompt;

		// 更新设置窗口的值
		ApiKeyTextBox->SetText(FText::FromString(TempApiKey));
		ApiUrlTextBox->SetText(FText::FromString(TempApiUrl));
		SystemPromptTextBox->SetText(FText::FromString(TempSystemPrompt));

		// 更新模型选择
		for (TSharedPtr<FModelInfo> ModelInfo : ModelList)
		{
			if (ModelInfo->Id == TempModel)
			{
				SelectedModel = ModelInfo;
				ModelComboBox->SetSelectedItem(SelectedModel);
				break;
			}
		}

		// 显示窗口
		FSlateApplication::Get().AddModalWindow(SettingsWindow.ToSharedRef(), AsShared());
	}

	return FReply::Handled();
}

TSharedRef<SWindow> SDeepseekAIChat::CreateSettingsWindow()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("Deepseek AI 设置")))
		.ClientSize(FVector2D(400, 400))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.HasCloseButton(true)
		.CreateTitleBar(true)
		.IsTopmostWindow(true)
		.IsInitiallyMaximized(false)
		.IsInitiallyMinimized(false)
		.ShouldPreserveAspectRatio(false);

	// 添加关闭按钮点击事件
	FOnWindowClosed WindowClosedDelegate;
	WindowClosedDelegate.BindSP(this, &SDeepseekAIChat::OnSettingsWindowClosed);
	Window->SetOnWindowClosed(WindowClosedDelegate);

	Window->SetContent(
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f))
		[
			SNew(SVerticalBox)

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
					.HintText(FText::FromString(TEXT("请输入您的API密钥...")))
				]
			]

			// 系统提示词
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
					.Text(FText::FromString(TEXT("系统提示词:")))
					.Font(FEditorStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SystemPromptTextBox, SMultiLineEditableTextBox)
					.HintText(FText::FromString(TEXT("请输入系统提示词...")))
					.AutoWrapText(true)
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
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return SelectedModel.IsValid()
								       ? FText::FromString(SelectedModel->Name)
								       : FText::FromString(TEXT("请选择模型"));
						})
					]
				]
			]

			// 填充空间
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)

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
					.OnClicked(this, &SDeepseekAIChat::OnCancelSettings)
				]
			]
		]
	);

	return Window;
}

FReply SDeepseekAIChat::OnApplySettings()
{
	// 获取新的设置
	FString NewApiKey = ApiKeyTextBox->GetText().ToString();
	FString NewApiUrl = ApiUrlTextBox->GetText().ToString();
	FString NewModel = SelectedModel.IsValid() ? SelectedModel->Id : TEXT("deepseek-chat");
	FString NewSystemPrompt = SystemPromptTextBox->GetText().ToString();

	// 检查是否有变化
	bool bHasChanges = (NewApiKey != CurrentApiKey) || (NewApiUrl != CurrentApiUrl) || (NewModel != CurrentModel) || (
		NewSystemPrompt != CurrentSystemPrompt);

	if (bHasChanges)
	{
		// 更新当前设置
		CurrentApiKey = NewApiKey;
		CurrentApiUrl = NewApiUrl;
		CurrentModel = NewModel;
		CurrentSystemPrompt = NewSystemPrompt;

		// 重新初始化OpenAI服务
		OpenAIService->Initialize(CurrentApiKey, CurrentModel, CurrentApiUrl);

		// 更新聊天历史中的系统消息
		if (ChatHistory.Num() > 0 && ChatHistory[0].Role == TEXT("system"))
		{
			ChatHistory[0].Content = CurrentSystemPrompt;
		}
		else
		{
			// 如果没有系统消息，添加一个
			ChatHistory.Insert(FOpenAIMessage(TEXT("system"), CurrentSystemPrompt), 0);
		}

		SaveSettings();

		// 添加系统消息
		ChatMessages.Add(MakeShared<FChatMessage>(TEXT("系统"), TEXT("设置已更新并保存"), false));
		ChatListView->RequestListRefresh();
	}

	// 关闭设置窗口
	if (SettingsWindow.IsValid())
	{
		SettingsWindow->RequestDestroyWindow();
		SettingsWindow.Reset();
	}

	return FReply::Handled();
}

FReply SDeepseekAIChat::OnCancelSettings()
{
	// 关闭设置窗口
	if (SettingsWindow.IsValid())
	{
		SettingsWindow->RequestDestroyWindow();
		SettingsWindow.Reset();
	}

	return FReply::Handled();
}

void SDeepseekAIChat::OnModelSelectionChanged(TSharedPtr<FModelInfo> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedModel = NewSelection;
		TempModel = SelectedModel->Id;
	}
}

TSharedRef<SWidget> SDeepseekAIChat::GenerateModelComboItem(TSharedPtr<FModelInfo> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(InItem->Name));
}

TSharedRef<ITableRow> SDeepseekAIChat::OnGenerateRow(TSharedPtr<FChatMessage> Message,
                                                     const TSharedRef<STableViewBase>& OwnerTable)
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

void SDeepseekAIChat::OnSettingsWindowClosed(const TSharedRef<SWindow>& Window)
{
	SettingsWindow.Reset();
}

void SDeepseekAIChat::SaveSettings()
{
	// 获取配置对象
	GConfig->SetString(
		TEXT("DeepseekAISettings"),
		TEXT("ApiKey"),
		*CurrentApiKey,
		GEngineIni
	);

	GConfig->SetString(
		TEXT("DeepseekAISettings"),
		TEXT("ApiUrl"),
		*CurrentApiUrl,
		GEngineIni
	);

	GConfig->SetString(
		TEXT("DeepseekAISettings"),
		TEXT("Model"),
		*CurrentModel,
		GEngineIni
	);

	GConfig->SetString(
		TEXT("DeepseekAISettings"),
		TEXT("SystemPrompt"),
		*CurrentSystemPrompt,
		GEngineIni
	);

	// 保存配置
	GConfig->Flush(false, GEngineIni);
}

void SDeepseekAIChat::LoadSettings()
{
	// 从配置文件加载设置
	FString LoadedApiKey;
	if (GConfig->GetString(
		TEXT("DeepseekAISettings"),
		TEXT("ApiKey"),
		LoadedApiKey,
		GEngineIni
	))
	{
		CurrentApiKey = LoadedApiKey;
	}

	FString LoadedApiUrl;
	if (GConfig->GetString(
		TEXT("DeepseekAISettings"),
		TEXT("ApiUrl"),
		LoadedApiUrl,
		GEngineIni
	))
	{
		CurrentApiUrl = LoadedApiUrl;
	}
	else if (CurrentApiUrl.IsEmpty())
	{
		// 如果没有保存过且当前为空，设置默认值
		CurrentApiUrl = TEXT("https://api.deepseek.com/chat/completions");
	}

	FString LoadedModel;
	if (GConfig->GetString(
		TEXT("DeepseekAISettings"),
		TEXT("Model"),
		LoadedModel,
		GEngineIni
	))
	{
		CurrentModel = LoadedModel;
	}
	else if (CurrentModel.IsEmpty())
	{
		// 如果没有保存过且当前为空，设置默认值
		CurrentModel = TEXT("deepseek-chat");
	}

	FString LoadedSystemPrompt;
	if (GConfig->GetString(
		TEXT("DeepseekAISettings"),
		TEXT("SystemPrompt"),
		LoadedSystemPrompt,
		GEngineIni
	))
	{
		CurrentSystemPrompt = LoadedSystemPrompt;
	}
	else if (CurrentSystemPrompt.IsEmpty())
	{
		// 如果没有保存过且当前为空，设置默认值
		CurrentSystemPrompt = TEXT("你是一个有用的AI助手，由Deepseek团队开发。请用中文回答问题，保持回答简洁明了。");
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
