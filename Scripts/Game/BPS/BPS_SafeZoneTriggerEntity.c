//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
//
// CHARACTER CORE
//
// - Native spherical trigger
// - Direct character detection
// - Vehicle occupant detection
// - Friendly enter/exit messages
// - Enemy countdown + execution
// - Character Safe Zone protection
// - Friendly fire blocking only when BOTH players are inside
// - Combat Lock after firing
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// Runtime state of one character.
//------------------------------------------------------------------------------------------------
class BPS_CharacterState
{
	IEntity m_Character;

	bool m_bDirectInside;
	bool m_bVehicleInside;
	bool m_bWasInside;

	int m_iIntruderStartTime;
	int m_iLastIntruderSecond;

	int m_iCombatLockStartTime;
	int m_iFriendlyFireMessageTime;

	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;

	int m_iLastAmmo;
	bool m_bWeaponInitialized;


	void BPS_CharacterState(IEntity character)
	{
		m_Character = character;

		m_bDirectInside = false;
		m_bVehicleInside = false;
		m_bWasInside = false;

		m_iIntruderStartTime = -1;
		m_iLastIntruderSecond = -1;

		m_iCombatLockStartTime = -1;
		m_iFriendlyFireMessageTime = -1;

		m_Weapon = null;
		m_Muzzle = null;

		m_iLastAmmo = -1;
		m_bWeaponInitialized = false;
	}


	bool IsInside()
	{
		return m_bDirectInside || m_bVehicleInside;
	}
}

class BPS_TurretWeaponState
{
	TurretControllerComponent m_Turret;
	IEntity m_Operator;

	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;

	int m_iLastAmmo;

	bool m_bInitialized;
	bool m_bSeen;


	void BPS_TurretWeaponState(
		TurretControllerComponent turret
	)
	{
		m_Turret = turret;

		m_Operator = null;

		m_Weapon = null;
		m_Muzzle = null;

		m_iLastAmmo = -1;

		m_bInitialized = false;
		m_bSeen = false;
	}
}


//------------------------------------------------------------------------------------------------
[EntityEditorProps(
	category: "BPS/Base Protection System",
	description: "Spherical Base Protection System Safe Zone."
)]
class BPS_SafeZoneTriggerEntityClass : SCR_BaseTriggerEntityClass
{
}


class BPS_SafeZoneTriggerEntity : SCR_BaseTriggerEntity
{
	protected static const float BPS_TRIGGER_UPDATE_RATE = 0.25;
	protected static const int BPS_LOGIC_INTERVAL_MS = 100;


	// =============================================================================================
	// CONFIGURATION OBJECTS
	// =============================================================================================

	[Attribute(
		"",
		UIWidgets.Object,
		"Faction configuration."
	)]
	protected ref BPS_FactionConfig m_FactionConfig =
		new BPS_FactionConfig();


	[Attribute(
		"",
		UIWidgets.Object,
		"Friendly enter/exit UI."
	)]
	protected ref BPS_FriendlyMessagesConfig m_FriendlyMessages =
		new BPS_FriendlyMessagesConfig();


	[Attribute(
		"",
		UIWidgets.Object,
		"Enemy intrusion configuration."
	)]
	protected ref BPS_IntruderConfig m_IntruderConfig =
		new BPS_IntruderConfig();


	[Attribute(
		"",
		UIWidgets.Object,
		"Combat Lock configuration."
	)]
	protected ref BPS_CombatConfig m_CombatConfig =
		new BPS_CombatConfig();


	[Attribute(
		"",
		UIWidgets.Object,
		"Friendly fire configuration."
	)]
	protected ref BPS_FriendlyFireConfig m_FriendlyFireConfig =
		new BPS_FriendlyFireConfig();


	// =============================================================================================
	// DEBUG
	// =============================================================================================

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Enable BPS logs."
	)]
	protected bool m_bDebug = true;


	// =============================================================================================
	// GLOBAL REGISTRY
	// =============================================================================================

	protected static ref array<BPS_SafeZoneTriggerEntity> s_aZones =
		new array<BPS_SafeZoneTriggerEntity>();


	// =============================================================================================
	// RUNTIME
	// =============================================================================================

	protected ref array<ref BPS_CharacterState> m_aStates =
		new array<ref BPS_CharacterState>();


	protected ref array<IEntity> m_aVehiclesInside =
		new array<IEntity>();


	protected ref array<IEntity> m_aOccupants =
		new array<IEntity>();


	protected ref array<BaseCompartmentSlot> m_aCompartments =
		new array<BaseCompartmentSlot>();


	protected ref array<ref BPS_TurretWeaponState> m_aTurretStates =
		new array<ref BPS_TurretWeaponState>();


	// =============================================================================================
	// INIT
	// =============================================================================================

	override void OnInit(IEntity owner)
	{
		super.OnInit(owner);


		EnsureConfig();


		SetUpdateRate(
			BPS_TRIGGER_UPDATE_RATE
		);


		if (
			Replication.IsRunning() &&
			!Replication.IsServer()
		)
		{
			EnablePeriodicQueries(false);

			return;
		}


		EnablePeriodicQueries(true);


		if (s_aZones.Find(this) < 0)
			s_aZones.Insert(this);


		GetGame().GetCallqueue().CallLater(
			BPS_LogicTick,
			BPS_LOGIC_INTERVAL_MS,
			true
		);


		DebugLog(
			string.Format(
				"INIT | Faction=%1 | Radius=%2",
				GetProtectedFactionKey(),
				GetSphereRadius()
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void EnsureConfig()
	{
		if (!m_FactionConfig)
			m_FactionConfig = new BPS_FactionConfig();


		if (!m_FriendlyMessages)
			m_FriendlyMessages = new BPS_FriendlyMessagesConfig();


		if (!m_IntruderConfig)
			m_IntruderConfig = new BPS_IntruderConfig();


		if (!m_CombatConfig)
			m_CombatConfig = new BPS_CombatConfig();


		if (!m_FriendlyFireConfig)
			m_FriendlyFireConfig = new BPS_FriendlyFireConfig();
	}


	// =============================================================================================
	// TRIGGER
	// =============================================================================================

	override bool ScriptedEntityFilterForQuery(IEntity ent)
	{
		if (!ent)
			return false;


		if (ChimeraCharacter.Cast(ent))
			return true;


		return GetCompartmentManager(ent) != null;
	}


	// =============================================================================================
	// ACTIVATE
	// =============================================================================================

	override void OnActivate(IEntity ent)
	{
		super.OnActivate(ent);


		if (!ent)
			return;


		ChimeraCharacter character =
			ChimeraCharacter.Cast(ent);


		// Direct character.
		if (character)
		{
			BPS_CharacterState state =
				GetOrCreateCharacterState(
					character
				);


			state.m_bDirectInside =
				true;


			return;
		}


		// Vehicle.
		if (!GetCompartmentManager(ent))
			return;


		if (m_aVehiclesInside.Find(ent) < 0)
		{
			m_aVehiclesInside.Insert(
				ent
			);
		}
	}


	// =============================================================================================
	// DEACTIVATE
	// =============================================================================================

	override void OnDeactivate(IEntity ent)
	{
		super.OnDeactivate(ent);


		if (!ent)
			return;


		ChimeraCharacter character =
			ChimeraCharacter.Cast(ent);


		if (character)
		{
			BPS_CharacterState state =
				FindCharacterState(
					character
				);


			if (state)
			{
				state.m_bDirectInside =
					false;
			}


			return;
		}


		int index =
			m_aVehiclesInside.Find(
				ent
			);


		if (index >= 0)
			m_aVehiclesInside.Remove(index);
	}


	// =============================================================================================
	// LOGIC LOOP
	// =============================================================================================

	protected void BPS_LogicTick()
	{
		// Reset source provided by vehicles.
		foreach (
			BPS_CharacterState state :
			m_aStates
		)
		{
			if (state)
				state.m_bVehicleInside = false;
		}


		// Turrets must be rediscovered each tick because:
		//
		// - occupants can switch seats
		// - weapons can change
		// - vehicles can leave
		foreach (
			BPS_TurretWeaponState turretState :
			m_aTurretStates
		)
		{
			if (turretState)
				turretState.m_bSeen = false;
		}


		ProcessVehicles();


		CleanupTurretStates();

		ProcessCharacters();
	}


	// =============================================================================================
	// VEHICLES
	// =============================================================================================

	protected void ProcessVehicles()
	{
		for (int i = m_aVehiclesInside.Count() - 1; i >= 0; i--)
		{
			IEntity vehicle =
				m_aVehiclesInside[i];


			if (!vehicle)
			{
				m_aVehiclesInside.Remove(i);
				continue;
			}


			SCR_BaseCompartmentManagerComponent manager =
				GetCompartmentManager(vehicle);


			if (!manager)
			{
				m_aVehiclesInside.Remove(i);
				continue;
			}


			ProcessVehicleOccupants(
				manager
			);


			ProcessVehicleTurrets(
				manager
			);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected void ProcessVehicleOccupants(
		SCR_BaseCompartmentManagerComponent manager
	)
	{
		m_aOccupants.Clear();


		manager.GetOccupants(
			m_aOccupants
		);


		foreach (
			IEntity occupant :
			m_aOccupants
		)
		{
			ChimeraCharacter character =
				ChimeraCharacter.Cast(
					occupant
				);


			if (!character)
				continue;


			BPS_CharacterState state =
				GetOrCreateCharacterState(
					character
				);


			state.m_bVehicleInside =
				true;
		}
	}


	// =============================================================================================
	// TURRET / MOUNTED WEAPONS
	// =============================================================================================

	protected void ProcessVehicleTurrets(
		SCR_BaseCompartmentManagerComponent manager
	)
	{
		m_aCompartments.Clear();


		manager.GetCompartments(
			m_aCompartments
		);


		foreach (
			BaseCompartmentSlot baseSlot :
			m_aCompartments
		)
		{
			ExtBaseCompartmentSlot slot =
				ExtBaseCompartmentSlot.Cast(
					baseSlot
				);


			if (!slot)
				continue;


			TurretControllerComponent turret =
				slot.GetAttachedTurret();


			if (!turret)
				continue;


			BPS_TurretWeaponState turretState =
				GetOrCreateTurretState(
					turret
				);


			turretState.m_bSeen =
				true;


			IEntity operator =
				slot.GetOccupant();


			// Empty turret.
			if (!operator)
			{
				ResetTurretWeaponState(
					turretState
				);

				continue;
			}


			if (!ChimeraCharacter.Cast(operator))
			{
				ResetTurretWeaponState(
					turretState
				);

				continue;
			}


			// New gunner/driver.
			if (
				turretState.m_Operator !=
				operator
			)
			{
				turretState.m_Operator =
					operator;


				ResetTurretWeaponState(
					turretState
				);
			}


			// BPS Combat Lock matters only to protected faction.
			if (!IsProtectedFaction(operator))
				continue;


			BPS_CharacterState operatorState =
				GetOrCreateCharacterState(
					operator
				);


			if (!operatorState.IsInside())
				continue;


			if (
				HasTurretFired(
					turretState
				)
			)
			{
				DebugLog(
					string.Format(
						"TURRET FIRE | Operator=%1",
						operator
					)
				);


				ApplyCombatLock(
					operatorState
				);
			}
		}
	}


	//------------------------------------------------------------------------------------------------
	protected bool HasTurretFired(
		BPS_TurretWeaponState state
	)
	{
		if (!state || !state.m_Turret)
			return false;


		BaseWeaponManagerComponent weaponManager =
			state.m_Turret.GetWeaponManager();


		if (!weaponManager)
		{
			ResetTurretWeaponState(
				state
			);

			return false;
		}


		BaseWeaponComponent weapon =
			SCR_WeaponLib.GetCurrentWeaponComponent(
				weaponManager
			);


		if (!weapon)
		{
			ResetTurretWeaponState(
				state
			);

			return false;
		}


		BaseMuzzleComponent muzzle =
			weapon.GetCurrentMuzzle();


		if (!muzzle)
		{
			ResetTurretWeaponState(
				state
			);

			return false;
		}


		int ammo =
			muzzle.GetAmmoCount();


		// Weapon/muzzle changed.
		if (
			state.m_Weapon != weapon ||
			state.m_Muzzle != muzzle
		)
		{
			state.m_Weapon = weapon;
			state.m_Muzzle = muzzle;

			state.m_iLastAmmo = ammo;

			state.m_bInitialized = true;

			return false;
		}


		if (!state.m_bInitialized)
		{
			state.m_iLastAmmo = ammo;
			state.m_bInitialized = true;

			return false;
		}


		bool fired =
			ammo <
			state.m_iLastAmmo;


		state.m_iLastAmmo =
			ammo;


		return fired;
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetTurretWeaponState(
		BPS_TurretWeaponState state
	)
	{
		if (!state)
			return;


		state.m_Weapon = null;
		state.m_Muzzle = null;

		state.m_iLastAmmo = -1;

		state.m_bInitialized = false;
	}


	//------------------------------------------------------------------------------------------------
	protected BPS_TurretWeaponState GetOrCreateTurretState(
		TurretControllerComponent turret
	)
	{
		foreach (
			BPS_TurretWeaponState state :
			m_aTurretStates
		)
		{
			if (
				state &&
				state.m_Turret == turret
			)
			{
				return state;
			}
		}


		BPS_TurretWeaponState newState =
			new BPS_TurretWeaponState(
				turret
			);


		m_aTurretStates.Insert(
			newState
		);


		return newState;
	}


	//------------------------------------------------------------------------------------------------
	protected void CleanupTurretStates()
	{
		for (int i = m_aTurretStates.Count() - 1; i >= 0; i--)
		{
			BPS_TurretWeaponState state =
				m_aTurretStates[i];


			if (
				!state ||
				!state.m_bSeen
			)
			{
				m_aTurretStates.Remove(i);
			}
		}
	}


	// =============================================================================================
	// CHARACTERS
	// =============================================================================================

	protected void ProcessCharacters()
	{
		for (int i = m_aStates.Count() - 1; i >= 0; i--)
		{
			BPS_CharacterState state =
				m_aStates[i];


			if (!state || !state.m_Character)
			{
				m_aStates.Remove(i);
				continue;
			}


			bool inside =
				state.IsInside();


			// ENTER
			if (
				inside &&
				!state.m_bWasInside
			)
			{
				state.m_bWasInside =
					true;


				OnCharacterEntered(
					state
				);
			}


			// EXIT
			else if (
				!inside &&
				state.m_bWasInside
			)
			{
				state.m_bWasInside =
					false;


				OnCharacterExited(
					state
				);
			}


			// ENEMY
			if (
				inside &&
				IsIntruderActive(state)
			)
			{
				ProcessIntruder(
					state
				);

				continue;
			}


			// FRIENDLY
			if (
				inside &&
				IsProtectedFaction(
					state.m_Character
				)
			)
			{
				// Personal weapon.
				if (
					GetCombatDuration() > 0 &&
					HasPersonalWeaponFired(
						state
					)
				)
				{
					ApplyCombatLock(
						state
					);
				}


				ProcessCombatLockExpiration(
					state
				);
			}


			if (
				!inside &&
				!IsIntruderActive(state) &&
				!IsCombatLocked(state)
			)
			{
				m_aStates.Remove(i);
			}
		}
	}


	// =============================================================================================
	// ENTER / EXIT
	// =============================================================================================

	protected void OnCharacterEntered(
		BPS_CharacterState state
	)
	{
		IEntity character =
			state.m_Character;


		DebugLog(
			string.Format(
				"ENTER | Entity=%1 | Faction=%2 | Direct=%3 | Vehicle=%4",
				character,
				GetFactionKey(character),
				state.m_bDirectInside,
				state.m_bVehicleInside
			)
		);


		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);

			InitializePersonalWeaponTracking(
				state
			);


			if (
				m_FriendlyMessages.IsEnabled()
			)
			{
				ShowMessage(
					character,
					m_FriendlyMessages.GetEnterTitle(),
					m_FriendlyMessages.GetEnterMessage(),
					m_FriendlyMessages.GetDuration()
				);
			}


			return;
		}


		StartIntruder(
			state
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void OnCharacterExited(
		BPS_CharacterState state
	)
	{
		IEntity character =
			state.m_Character;


		ResetPersonalWeaponTracking(
			state
		);


		if (IsIntruderActive(state))
		{
			ResetIntruder(
				state
			);


			ShowMessage(
				character,
				m_IntruderConfig.GetExitTitle(),
				m_IntruderConfig.GetExitMessage(),
				m_IntruderConfig.GetExitMessageDuration()
			);


			return;
		}


		if (
			IsProtectedFaction(character) &&
			m_FriendlyMessages.IsEnabled()
		)
		{
			ShowMessage(
				character,
				m_FriendlyMessages.GetExitTitle(),
				m_FriendlyMessages.GetExitMessage(),
				m_FriendlyMessages.GetDuration()
			);
		}
	}


	// =============================================================================================
	// PERSONAL WEAPON
	// =============================================================================================

	protected bool HasPersonalWeaponFired(
		BPS_CharacterState state
	)
	{
		ChimeraCharacter character =
			ChimeraCharacter.Cast(
				state.m_Character
			);


		if (!character)
			return false;


		BaseWeaponComponent weapon =
			SCR_WeaponLib.GetCurrentWeaponComponent(
				character
			);


		if (!weapon)
		{
			ResetPersonalWeaponTracking(
				state
			);

			return false;
		}


		BaseMuzzleComponent muzzle =
			weapon.GetCurrentMuzzle();


		if (!muzzle)
		{
			ResetPersonalWeaponTracking(
				state
			);

			return false;
		}


		int ammo =
			muzzle.GetAmmoCount();


		if (
			state.m_Weapon != weapon ||
			state.m_Muzzle != muzzle
		)
		{
			state.m_Weapon = weapon;
			state.m_Muzzle = muzzle;

			state.m_iLastAmmo = ammo;

			state.m_bWeaponInitialized = true;

			return false;
		}


		if (!state.m_bWeaponInitialized)
		{
			state.m_iLastAmmo = ammo;
			state.m_bWeaponInitialized = true;

			return false;
		}


		bool fired =
			ammo <
			state.m_iLastAmmo;


		state.m_iLastAmmo =
			ammo;


		return fired;
	}


	//------------------------------------------------------------------------------------------------
	protected void InitializePersonalWeaponTracking(
		BPS_CharacterState state
	)
	{
		ResetPersonalWeaponTracking(
			state
		);


		HasPersonalWeaponFired(
			state
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetPersonalWeaponTracking(
		BPS_CharacterState state
	)
	{
		state.m_Weapon = null;
		state.m_Muzzle = null;

		state.m_iLastAmmo = -1;

		state.m_bWeaponInitialized = false;
	}


	// =============================================================================================
	// COMBAT LOCK
	// =============================================================================================

	protected void ApplyCombatLock(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		if (GetCombatDuration() <= 0)
			return;


		bool alreadyLocked =
			IsCombatLocked(
				state
			);


		state.m_iCombatLockStartTime =
			System.GetTickCount();


		if (!alreadyLocked)
		{
			ShowMessage(
				state.m_Character,
				m_CombatConfig.GetLockTitle(),
				m_CombatConfig.GetLockMessage(),
				m_CombatConfig.GetMessageDuration()
			);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsCombatLocked(
		BPS_CharacterState state
	)
	{
		if (
			!state ||
			state.m_iCombatLockStartTime < 0
		)
		{
			return false;
		}


		int durationMs =
			GetCombatDuration() *
			1000;


		return (
			System.GetTickCount(
				state.m_iCombatLockStartTime
			)
			<
			durationMs
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ProcessCombatLockExpiration(
		BPS_CharacterState state
	)
	{
		if (
			state.m_iCombatLockStartTime < 0
		)
		{
			return;
		}


		if (IsCombatLocked(state))
			return;


		state.m_iCombatLockStartTime =
			-1;


		ShowMessage(
			state.m_Character,
			m_CombatConfig.GetRestoredTitle(),
			m_CombatConfig.GetRestoredMessage(),
			m_CombatConfig.GetMessageDuration()
		);
	}


	// =============================================================================================
	// INTRUDER
	// =============================================================================================

	protected void StartIntruder(
		BPS_CharacterState state
	)
	{
		state.m_iIntruderStartTime =
			System.GetTickCount();


		state.m_iLastIntruderSecond =
			GetIntruderDelay();


		ShowMessageParam1(
			state.m_Character,
			m_IntruderConfig.GetWarningTitle(),
			m_IntruderConfig.GetWarningMessage(),
			m_IntruderConfig.GetCountdownMessageDuration(),
			GetIntruderDelay()
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ProcessIntruder(
		BPS_CharacterState state
	)
	{
		if (!state.IsInside())
			return;


		if (
			IsProtectedFaction(
				state.m_Character
			)
		)
		{
			ResetIntruder(state);

			return;
		}


		int elapsed =
			System.GetTickCount(
				state.m_iIntruderStartTime
			);


		int total =
			GetIntruderDelay() *
			1000;


		int remainingMs =
			total -
			elapsed;


		if (remainingMs <= 0)
		{
			KillIntruder(
				state
			);

			return;
		}


		int remaining =
			(remainingMs + 999) /
			1000;


		if (
			remaining ==
			state.m_iLastIntruderSecond
		)
		{
			return;
		}


		state.m_iLastIntruderSecond =
			remaining;


		ShowMessageParam1(
			state.m_Character,
			m_IntruderConfig.GetWarningTitle(),
			m_IntruderConfig.GetWarningMessage(),
			m_IntruderConfig.GetCountdownMessageDuration(),
			remaining
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void KillIntruder(
		BPS_CharacterState state
	)
	{
		if (
			!state ||
			!state.IsInside()
		)
		{
			return;
		}


		SCR_DamageManagerComponent manager =
			SCR_DamageManagerComponent.GetDamageManager(
				state.m_Character
			);


		if (!manager)
			return;


		if (manager.IsDestroyed())
		{
			ResetIntruder(state);
			return;
		}


		ref Instigator instigator =
			Instigator.CreateInstigatorGM();


		if (instigator)
		{
			manager.Kill(
				instigator
			);
		}


		if (!manager.IsDestroyed())
		{
			manager.SetHealthScaled(
				0
			);
		}


		if (
			manager.IsDestroyed() ||
			manager.GetHealthScaled() <= 0
		)
		{
			ResetIntruder(
				state
			);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsIntruderActive(
		BPS_CharacterState state
	)
	{
		return (
			state &&
			state.m_iIntruderStartTime >= 0
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetIntruder(
		BPS_CharacterState state
	)
	{
		state.m_iIntruderStartTime = -1;
		state.m_iLastIntruderSecond = -1;
	}


	// =============================================================================================
	// DAMAGE
	// =============================================================================================

	static bool BPS_ShouldBlockDamage(
		IEntity victim,
		notnull BaseDamageContext damageContext
	)
	{
		foreach (
			BPS_SafeZoneTriggerEntity zone :
			s_aZones
		)
		{
			if (
				zone &&
				zone.ShouldBlockDamage(
					victim,
					damageContext
				)
			)
			{
				return true;
			}
		}


		return false;
	}


	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockDamage(
		IEntity victim,
		notnull BaseDamageContext damageContext
	)
	{
		if (!ChimeraCharacter.Cast(victim))
			return false;


		if (!IsProtectedFaction(victim))
			return false;


		BPS_CharacterState victimState =
			FindCharacterState(
				victim
			);


		// IMPORTANT:
		//
		// Friendly outside the Safe Zone:
		// no state inside -> NORMAL DAMAGE.
		if (
			!victimState ||
			!victimState.IsInside()
		)
		{
			return false;
		}


		IEntity attacker = null;


		if (damageContext.instigator)
		{
			attacker =
				damageContext.instigator.GetInstigatorEntity();
		}


		// Friendly fire is blocked because the VICTIM is inside.
		//
		// If victim is outside this code was already exited above.
		if (
			m_FriendlyFireConfig.IsEnabled() &&
			attacker &&
			attacker != victim &&
			IsProtectedFaction(attacker)
		)
		{
			BPS_CharacterState attackerState =
				FindCharacterState(
					attacker
				);


			if (
				attackerState &&
				attackerState.IsInside()
			)
			{
				ShowFriendlyFireMessage(
					attacker,
					attackerState
				);
			}


			return true;
		}


		// Combat Lock removes normal protection.
		//
		// Friendly fire was processed BEFORE this.
		if (IsCombatLocked(victimState))
			return false;


		return true;
	}


	// =============================================================================================
	// FRIENDLY FIRE MESSAGE
	// =============================================================================================

	protected void ShowFriendlyFireMessage(
		IEntity attacker,
		BPS_CharacterState state
	)
	{
		int cooldown =
			m_FriendlyFireConfig.GetWarningCooldown() *
			1000;


		if (
			state.m_iFriendlyFireMessageTime >= 0 &&
			System.GetTickCount(
				state.m_iFriendlyFireMessageTime
			)
			<
			cooldown
		)
		{
			return;
		}


		state.m_iFriendlyFireMessageTime =
			System.GetTickCount();


		ShowMessage(
			attacker,
			m_FriendlyFireConfig.GetTitle(),
			m_FriendlyFireConfig.GetMessage(),
			m_FriendlyFireConfig.GetMessageDuration()
		);
	}


	// =============================================================================================
	// VEHICLE HELPERS
	// =============================================================================================

	protected SCR_BaseCompartmentManagerComponent GetCompartmentManager(
		IEntity ent
	)
	{
		if (!ent)
			return null;


		return SCR_BaseCompartmentManagerComponent.Cast(
			ent.FindComponent(
				SCR_BaseCompartmentManagerComponent
			)
		);
	}


	// =============================================================================================
	// FACTION
	// =============================================================================================

	protected FactionKey GetProtectedFactionKey()
	{
		return m_FactionConfig.GetProtectedFactionKey();
	}


	protected FactionKey GetFactionKey(
		IEntity ent
	)
	{
		Faction faction =
			SCR_Faction.GetEntityFaction(
				ent
			);


		if (!faction)
			return "";


		return faction.GetFactionKey();
	}


	protected bool IsProtectedFaction(
		IEntity ent
	)
	{
		return (
			GetFactionKey(ent) ==
			GetProtectedFactionKey()
		);
	}


	// =============================================================================================
	// CONFIG HELPERS
	// =============================================================================================

	protected int GetIntruderDelay()
	{
		return m_IntruderConfig.GetKillDelaySeconds();
	}


	protected float GetCombatDuration()
	{
		return m_CombatConfig.GetDuration();
	}


	// =============================================================================================
	// CHARACTER STATE
	// =============================================================================================

	protected BPS_CharacterState GetOrCreateCharacterState(
		IEntity character
	)
	{
		BPS_CharacterState existing =
			FindCharacterState(
				character
			);


		if (existing)
			return existing;


		BPS_CharacterState state =
			new BPS_CharacterState(
				character
			);


		m_aStates.Insert(
			state
		);


		return state;
	}


	protected BPS_CharacterState FindCharacterState(
		IEntity character
	)
	{
		foreach (
			BPS_CharacterState state :
			m_aStates
		)
		{
			if (
				state &&
				state.m_Character == character
			)
			{
				return state;
			}
		}


		return null;
	}


	// =============================================================================================
	// UI
	// =============================================================================================

	protected SCR_PlayerController GetController(
		IEntity character
	)
	{
		PlayerManager manager =
			GetGame().GetPlayerManager();


		if (!manager)
			return null;


		int playerId =
			manager.GetPlayerIdFromControlledEntity(
				character
			);


		if (playerId <= 0)
			return null;


		return SCR_PlayerController.Cast(
			manager.GetPlayerController(
				playerId
			)
		);
	}


	protected void ShowMessage(
		IEntity character,
		string title,
		string message,
		float duration
	)
	{
		if (message == "")
			return;


		SCR_PlayerController controller =
			GetController(
				character
			);


		if (!controller)
			return;


		controller.BPS_ShowMessage(
			title,
			message,
			duration
		);
	}


	protected void ShowMessageParam1(
		IEntity character,
		string title,
		string message,
		float duration,
		int param1
	)
	{
		SCR_PlayerController controller =
			GetController(
				character
			);


		if (!controller)
			return;


		controller.BPS_ShowMessageParam1(
			title,
			message,
			duration,
			param1
		);
	}


	// =============================================================================================
	// DEBUG
	// =============================================================================================

	protected void DebugLog(
		string text
	)
	{
		if (!m_bDebug)
			return;


		PrintFormat(
			"[BPS] %1",
			text
		);
	}


	// =============================================================================================
	// CLEANUP
	// =============================================================================================

	void ~BPS_SafeZoneTriggerEntity()
	{
		if (GetGame())
		{
			ScriptCallQueue queue =
				GetGame().GetCallqueue();


			if (queue)
			{
				queue.Remove(
					BPS_LogicTick
				);
			}
		}


		int index =
			s_aZones.Find(this);


		if (index >= 0)
			s_aZones.Remove(index);
	}
}