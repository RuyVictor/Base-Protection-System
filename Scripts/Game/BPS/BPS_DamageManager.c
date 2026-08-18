//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
//
// Character damage interception.
//------------------------------------------------------------------------------------------------

modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(
		notnull BaseDamageContext damageContext
	)
	{
		// Damage authority only.
		if (
			!Replication.IsRunning() ||
			Replication.IsServer()
		)
		{
			IEntity owner =
				GetOwner();


			if (
				owner &&
				BPS_SafeZoneTriggerEntity.BPS_ShouldBlockDamage(
					owner,
					damageContext
				)
			)
			{
				return true;
			}
		}


		return super.HijackDamageHandling(
			damageContext
		);
	}
}