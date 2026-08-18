//------------------------------------------------------------------------------------------------
// BPS - Character damage protection
//------------------------------------------------------------------------------------------------

modded class SCR_CharacterDamageManagerComponent
{
	override bool HijackDamageHandling(
		notnull BaseDamageContext damageContext
	)
	{
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