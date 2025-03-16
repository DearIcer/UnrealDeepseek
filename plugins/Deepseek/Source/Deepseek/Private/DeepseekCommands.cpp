// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeepseekCommands.h"

#define LOCTEXT_NAMESPACE "FDeepseekModule"

void FDeepseekCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Deepseek", "Bring up Deepseek window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
