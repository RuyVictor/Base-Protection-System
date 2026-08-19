//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
//
// Damage interception for characters and vehicles.
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// CHARACTER DAMAGE
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
			IEntity owner = GetOwner();

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


//------------------------------------------------------------------------------------------------
// VEHICLE DAMAGE
//
// This hook does NOT make friendly vehicles generally invulnerable.
// BPS only returns true here for protected-faction friendly fire while the
// victim vehicle is actually tracked inside the Safe Zone.
//------------------------------------------------------------------------------------------------
modded class SCR_VehicleDamageManagerComponent
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
			IEntity owner = GetOwner();

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
