#include "AtlasFileWork.h"
#include "Tasks/Pipe.h"
#include "Async/TaskGraphInterfaces.h"
namespace AtlasFileWork
{
	static UE::Tasks::FPipe Pipe(TEXT("AtlasFileIO"));
	static bool bStopping = false;
	void Enqueue(TUniqueFunction<void()> Work)
	{
		check(IsInGameThread());
		if (bStopping) return;
		Pipe.Launch(UE_SOURCE_LOCATION, [Work = MoveTemp(Work)]() mutable { Work(); });
	}
	void Drain()
	{
		check(IsInGameThread());
		do
		{
			Pipe.WaitUntilEmpty();
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		} while (Pipe.HasWork());
	}
	void Shutdown()
	{
		bStopping = true;
		Pipe.WaitUntilEmpty();
		if (IsInGameThread()) FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}
}
