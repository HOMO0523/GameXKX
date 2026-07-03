#include "MyBlueprintFunctionLibrary.h"

#include "Kismet/KismetMathLibrary.h"

namespace
{
	bool IsAllZero(const TArray<int32>& Array)
	{
		for (const int32 Value : Array)
		{
			if (Value != 0)
			{
				return false;
			}
		}
		return true;
	}
}

int32 UMyBlueprintFunctionLibrary::FunCL_GetRandomArrayToIndex(TArray<int32> Value)
{
	if (Value.IsEmpty() || IsAllZero(Value))
	{
		return 0;
	}

	int32 Sum = 0;
	for (const int32 Weight : Value)
	{
		if (Weight > 0)
		{
			Sum += Weight;
		}
	}

	if (Sum <= 0)
	{
		return 0;
	}

	const int32 Random = UKismetMathLibrary::RandomIntegerInRange(0, Sum - 1);
	int32 Current = 0;
	for (int32 Index = 0; Index < Value.Num(); ++Index)
	{
		if (Value[Index] > 0)
		{
			Current += Value[Index];
		}
		if (Random < Current)
		{
			return Index;
		}
	}

	return 0;
}
