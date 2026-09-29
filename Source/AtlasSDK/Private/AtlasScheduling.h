// Copyright Atlas Platform. All Rights Reserved.
#pragma once
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

// Network work follows real time, independently of editor/PIE worlds and gameplay pause.
// The engine's core ticker executes these callbacks on the game thread.
namespace AtlasScheduling
{
	inline void Cancel(FTSTicker::FDelegateHandle& Handle)
	{
		FTSTicker::RemoveTicker(Handle);
		Handle.Reset();
	}

	template<typename TObject, typename TCallback>
	FTSTicker::FDelegateHandle Schedule(TObject* Owner, double DelaySeconds, TCallback Callback, FTSTicker& Ticker = FTSTicker::GetCoreTicker())
	{
		const TWeakObjectPtr<TObject> WeakOwner(Owner);
		const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.0, DelaySeconds);
		return Ticker.AddTicker(TEXT("AtlasNetworkDeadline"), 0.0f,
			[WeakOwner, Deadline, Callback](float)
			{
				if (!WeakOwner.IsValid()) return false;
				if (FPlatformTime::Seconds() < Deadline) return true;
				TStrongObjectPtr<TObject> KeepAlive(WeakOwner.Get());
				Callback(KeepAlive.Get());
				return false;
			});
	}
}
