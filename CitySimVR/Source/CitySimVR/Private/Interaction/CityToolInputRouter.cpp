#include "Interaction/CityToolInputRouter.h"

ECityControllerHand FCityToolInputRouter::OtherHand(const ECityControllerHand Hand)
{
	return Hand == ECityControllerHand::Left ? ECityControllerHand::Right : ECityControllerHand::Left;
}

ECityTriggerRoute FCityToolInputRouter::RouteTrigger(const ECityControllerHand PressedHand,
                                                     const ECityControllerHand DominantHand, const bool bPaletteOpen,
                                                     const bool bPaletteHit)
{
	if (PressedHand != DominantHand)
	{
		return ECityTriggerRoute::None;
	}

	if (bPaletteOpen)
		return bPaletteHit ? ECityTriggerRoute::SelectPalette : ECityTriggerRoute::None;
	return ECityTriggerRoute::ConfirmTool;
}
