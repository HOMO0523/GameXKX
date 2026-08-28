#include "Interaction/GameXXKInteractionRules.h"

TOptional<FGameXXKInteractionCandidate> FGameXXKInteractionRules::Choose(
	const TArray<FGameXXKInteractionCandidate>& Candidates)
{
	TArray<FGameXXKInteractionCandidate> Eligible;
	for (const FGameXXKInteractionCandidate& Candidate : Candidates)
	{
		if (Candidate.InteractionId.IsNone()
			|| !FMath::IsFinite(Candidate.Distance)
			|| Candidate.Distance < 0.0f)
		{
			continue;
		}
		Eligible.Add(Candidate);
	}
	Eligible.Sort([](const FGameXXKInteractionCandidate& Left, const FGameXXKInteractionCandidate& Right)
	{
		if (Left.Priority != Right.Priority) return Left.Priority > Right.Priority;
		if (!FMath::IsNearlyEqual(Left.Distance, Right.Distance)) return Left.Distance < Right.Distance;
		return Left.InteractionId.LexicalLess(Right.InteractionId);
	});
	return Eligible.IsEmpty()
		? TOptional<FGameXXKInteractionCandidate>()
		: TOptional<FGameXXKInteractionCandidate>(Eligible[0]);
}
