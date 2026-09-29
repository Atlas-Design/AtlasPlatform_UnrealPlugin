#pragma once
#include "CoreMinimal.h"
namespace AtlasFileWork
{
	// Serial worker pipe bounds simultaneous disk work; UObject access stays on the game thread.
	void Enqueue(TUniqueFunction<void()> Work);
	void Drain();
	void Shutdown();
}
