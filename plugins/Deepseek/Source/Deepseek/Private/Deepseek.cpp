// Copyright Epic Games, Inc. All Rights Reserved.

#include "Deepseek.h"
#include "DeepseekStyle.h"
#include "DeepseekCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "SDeepseekAIChat.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

static const FName DeepseekTabName("Deepseek");

// 在这里设置你的OpenAI API密钥
static const FString OpenAIApiKey = TEXT("sk-823c4c63ef214db79df81e9a310002e9");
static const FString OpenAIModel = TEXT("deepseek-chat");

#define LOCTEXT_NAMESPACE "FDeepseekModule"

void FDeepseekModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FDeepseekStyle::Initialize();
	FDeepseekStyle::ReloadTextures();

	FDeepseekCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FDeepseekCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FDeepseekModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDeepseekModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DeepseekTabName, FOnSpawnTab::CreateRaw(this, &FDeepseekModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FDeepseekTabTitle", "Deepseek AI"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// 检查API密钥是否已设置
	if (OpenAIApiKey == TEXT("your_api_key_here"))
	{
		FNotificationInfo Info(LOCTEXT("APIKeyNotSet", "请在Deepseek.cpp文件中设置您的OpenAI API密钥"));
		Info.bUseLargeFont = true;
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FDeepseekModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FDeepseekStyle::Shutdown();

	FDeepseekCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DeepseekTabName);
}

TSharedRef<SDockTab> FDeepseekModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDeepseekAIChat)
			.ApiKey(OpenAIApiKey)
			.Model(OpenAIModel)
		];
}

void FDeepseekModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(DeepseekTabName);
}

void FDeepseekModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FDeepseekCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FDeepseekCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDeepseekModule, Deepseek)