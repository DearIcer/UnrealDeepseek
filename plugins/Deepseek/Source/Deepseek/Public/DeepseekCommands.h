// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "DeepseekStyle.h"

class FDeepseekCommands : public TCommands<FDeepseekCommands>
{
public:

	FDeepseekCommands()
		: TCommands<FDeepseekCommands>(TEXT("Deepseek"), NSLOCTEXT("Contexts", "Deepseek", "Deepseek Plugin"), NAME_None, FDeepseekStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};