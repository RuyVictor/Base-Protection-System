//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
// Damage interception for characters and vehicles.
//------------------------------------------------------------------------------------------------
modded class SCR_CharacterDamageManagerComponent
{
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			IEntity owner = GetOwner();
			if (owner && BPS_SafeZoneTriggerEntity.BPS_ShouldBlockDamage(owner, damageContext))
				return true;
		}

		return super.HijackDamageHandling(damageContext);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_VehicleDamageManagerComponent
{
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			IEntity owner = GetOwner();
			if (owner && BPS_SafeZoneTriggerEntity.BPS_ShouldBlockDamage(owner, damageContext))
				return true;
		}

		return super.HijackDamageHandling(damageContext);
	}
}
