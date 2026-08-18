//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
//
// Character-only core implementation.
//
// Features:
// - Native spherical trigger
// - Protected faction
// - Friendly enter/exit messages
// - Enemy countdown + execution
// - Safe Zone damage protection
// - Friendly fire blocking
// - Combat Lock when firing
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// Runtime state for one character.
//------------------------------------------------------------------------------------------------
class BPS_CharacterState
{
	IEntity m_Character;

	bool m_bInside;

	// Enemy intrusion
	int m_iIntruderStartTime;
	int m_iLastIntruderSecond;

	// Combat lock
	int m_iCombatLockStartTime;

	// Weapon tracking
	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;

	int m_iLastAmmo;
	bool m_bWeaponInitialized;


	//------------------------------------------------------------------------------------------------
	void BPS_CharacterState(IEntity character)
	{
		m_Character = character;

		m_bInside = false;

		m_iIntruderStartTime = -1;
		m_iLastIntruderSecond = -1;

		m_iCombatLockStartTime = -1;

		m_Weapon = null;
		m_Muzzle = null;

		m_iLastAmmo = -1;
		m_bWeaponInitialized = false;
	}
}


//------------------------------------------------------------------------------------------------
[EntityEditorProps(
	category: "BPS/Base Protection System",
	description: "Spherical character Safe Zone."
)]
class BPS_SafeZoneTriggerEntityClass : SCR_BaseTriggerEntityClass
{
}


//------------------------------------------------------------------------------------------------
class BPS_SafeZoneTriggerEntity : SCR_BaseTriggerEntity
{
	// =============================================================================================
	// INTERNAL CONSTANTS
	//
	// Not exposed in prefab.
	// =============================================================================================

	protected static const float BPS_TRIGGER_UPDATE_RATE = 0.25;

	protected static const int BPS_LOGIC_INTERVAL_MS = 100;

	protected static const float BPS_COUNTDOWN_HINT_DURATION = 1.1;


	// =============================================================================================
	// ACTIVE ZONES
	//
	// Used by the DamageManager hook.
	// =============================================================================================

	protected static ref array<BPS_SafeZoneTriggerEntity> s_aZones =
		new array<BPS_SafeZoneTriggerEntity>();


	// =============================================================================================
	// FACTION
	// =============================================================================================

	[Attribute(
		"US",
		UIWidgets.EditBox,
		"Protected faction key. Vanilla examples: US, USSR."
	)]
	protected FactionKey m_sProtectedFactionKey;


	// =============================================================================================
	// FRIENDLY UI
	// =============================================================================================

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show a message when a friendly player enters or leaves the Safe Zone."
	)]
	protected bool m_bShowFriendlyEnterExitMessages;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Friendly enter title. Supports localization keys."
	)]
	protected string m_sFriendlyEnterTitle;


	[Attribute(
		"You entered a safe zone.",
		UIWidgets.EditBox,
		"Friendly enter message. Supports localization keys."
	)]
	protected string m_sFriendlyEnterMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Friendly exit title. Supports localization keys."
	)]
	protected string m_sFriendlyExitTitle;


	[Attribute(
		"You left the safe zone.",
		UIWidgets.EditBox,
		"Friendly exit message. Supports localization keys."
	)]
	protected string m_sFriendlyExitMessage;


	// =============================================================================================
	// ENEMY INTRUSION
	// =============================================================================================

	[Attribute(
		"10",
		UIWidgets.EditBox,
		"Seconds an enemy may remain inside before being killed."
	)]
	protected int m_iEnemyKillDelaySeconds;


	[Attribute(
		"WARNING - ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Enemy warning title. Supports localization keys."
	)]
	protected string m_sEnemyWarningTitle;


	[Attribute(
		"You are inside an enemy Safe Zone. Leave immediately. You will be killed in %1 seconds.",
		UIWidgets.EditBox,
		"Enemy warning. Supports localization keys. %1 = seconds remaining."
	)]
	protected string m_sEnemyWarningMessage;


	[Attribute(
		"ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Enemy exit title. Supports localization keys."
	)]
	protected string m_sEnemyExitTitle;


	[Attribute(
		"You left the enemy Safe Zone. Elimination cancelled.",
		UIWidgets.EditBox,
		"Enemy exit message. Supports localization keys."
	)]
	protected string m_sEnemyExitMessage;


	// =============================================================================================
	// COMBAT LOCK
	// =============================================================================================

	[Attribute(
		"15",
		UIWidgets.EditBox,
		"Seconds without Safe Zone protection after firing. Set 0 to disable Combat Lock."
	)]
	protected float m_fCombatLockSeconds;


	[Attribute(
		"SAFE ZONE - COMBAT",
		UIWidgets.EditBox,
		"Combat Lock title. Supports localization keys."
	)]
	protected string m_sCombatLockTitle;


	[Attribute(
		"You fired inside the Safe Zone. Your protection has been temporarily disabled.",
		UIWidgets.EditBox,
		"Combat Lock message. Supports localization keys."
	)]
	protected string m_sCombatLockMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Protection restored title. Supports localization keys."
	)]
	protected string m_sProtectionRestoredTitle;


	[Attribute(
		"Your Safe Zone protection has been restored.",
		UIWidgets.EditBox,
		"Protection restored message. Supports localization keys."
	)]
	protected string m_sProtectionRestoredMessage;


	// =============================================================================================
	// FRIENDLY FIRE
	// =============================================================================================

	[Attribute(
		"FRIENDLY FIRE BLOCKED",
		UIWidgets.EditBox,
		"Friendly fire warning title. Supports localization keys."
	)]
	protected string m_sFriendlyFireTitle;


	[Attribute(
		"You cannot damage allies inside the Safe Zone.",
		UIWidgets.EditBox,
		"Friendly fire warning message. Supports localization keys."
	)]
	protected string m_sFriendlyFireMessage;


	// =============================================================================================
	// UI
	// =============================================================================================

	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Normal UI message duration."
	)]
	protected float m_fMessageDuration;


	// =============================================================================================
	// DEBUG
	// =============================================================================================

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Enable BPS debug logging."
	)]
	protected bool m_bDebug;


	// =============================================================================================
	// RUNTIME
	// =============================================================================================

	protected ref array<ref BPS_CharacterState> m_aStates =
		new array<ref BPS_CharacterState>();


	// =============================================================================================
	// INIT
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	override void OnInit(IEntity owner)
	{
		super.OnInit(owner);


		if (m_sProtectedFactionKey == "URSS")
		{
			m_sProtectedFactionKey = "USSR";

			Print(
				"[BPS] URSS converted to USSR.",
				LogLevel.WARNING
			);
		}


		if (m_iEnemyKillDelaySeconds < 1)
			m_iEnemyKillDelaySeconds = 1;


		if (m_fCombatLockSeconds < 0)
			m_fCombatLockSeconds = 0;


		SetUpdateRate(
			BPS_TRIGGER_UPDATE_RATE
		);


		// Client does not need to perform trigger queries.
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
				"INIT | Faction=%1 | SphereRadius=%2",
				m_sProtectedFactionKey,
				GetSphereRadius()
			)
		);
	}


	// =============================================================================================
	// FILTER
	//
	// Character-only version.
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	override bool ScriptedEntityFilterForQuery(IEntity ent)
	{
		return ChimeraCharacter.Cast(ent) != null;
	}


	// =============================================================================================
	// ENTER
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	override void OnActivate(IEntity ent)
	{
		super.OnActivate(ent);
	
		if (!ent)
			return;
	
		ChimeraCharacter character = ChimeraCharacter.Cast(ent);
	
		if (!character)
			return;
	
		BPS_CharacterState state = GetOrCreateState(character);
	
		state.m_bInside = true;
	
		DebugLog(
			string.Format(
				"ENTER | Entity=%1 | Faction=%2 | ProtectedFaction=%3",
				character,
				GetFactionKey(character),
				m_sProtectedFactionKey
			)
		);
	
		// FRIENDLY
		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);
			InitializeWeaponTracking(state);
	
			if (m_bShowFriendlyEnterExitMessages)
			{
				ShowMessage(
					character,
					m_sFriendlyEnterTitle,
					m_sFriendlyEnterMessage
				);
			}
	
			return;
		}
	
		// ENEMY
		StartIntruderCountdown(state);
	}


	// =============================================================================================
	// EXIT
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	override void OnDeactivate(IEntity ent)
	{
		super.OnDeactivate(ent);
	
		if (!ent)
			return;
	
		BPS_CharacterState state = FindState(ent);
	
		if (!state)
			return;
	
		DebugLog(
			string.Format(
				"EXIT | Entity=%1 | Intruder=%2",
				ent,
				IsIntruderActive(state)
			)
		);
	
		state.m_bInside = false;
	
		ResetWeaponTracking(state);
	
		// ENEMY LEFT
		if (IsIntruderActive(state))
		{
			ResetIntruder(state);
	
			ShowMessage(
				ent,
				m_sEnemyExitTitle,
				m_sEnemyExitMessage
			);
	
			if (!IsCombatLocked(state))
				RemoveState(state);
	
			return;
		}
	
		// FRIENDLY LEFT
		if (
			IsProtectedFaction(ent) &&
			m_bShowFriendlyEnterExitMessages
		)
		{
			ShowMessage(
				ent,
				m_sFriendlyExitTitle,
				m_sFriendlyExitMessage
			);
		}
	
		if (!IsCombatLocked(state))
			RemoveState(state);
	}


	// =============================================================================================
	// MAIN SERVER LOOP
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected void BPS_LogicTick()
	{
		for (int i = m_aStates.Count() - 1; i >= 0; i--)
		{
			BPS_CharacterState state =
				m_aStates[i];


			if (!state)
			{
				m_aStates.Remove(i);

				continue;
			}


			IEntity character =
				state.m_Character;


			if (!character)
			{
				m_aStates.Remove(i);

				continue;
			}


			// =====================================================================================
			// ENEMY COUNTDOWN
			// =====================================================================================

			if (IsIntruderActive(state))
			{
				ProcessIntruder(
					state
				);

				continue;
			}


			// =====================================================================================
			// FRIENDLY WEAPON DETECTION
			// =====================================================================================

			if (
				state.m_bInside &&
				IsProtectedFaction(character) &&
				m_fCombatLockSeconds > 0
			)
			{
				if (HasFired(state))
				{
					ApplyCombatLock(
						state
					);
				}
			}


			// =====================================================================================
			// COMBAT LOCK FINISHED
			// =====================================================================================

			if (
				state.m_iCombatLockStartTime >= 0 &&
				!IsCombatLocked(state)
			)
			{
				state.m_iCombatLockStartTime = -1;


				if (state.m_bInside)
				{
					ShowMessage(
						character,
						m_sProtectionRestoredTitle,
						m_sProtectionRestoredMessage
					);
				}
			}


			// =====================================================================================
			// CLEANUP
			// =====================================================================================

			if (
				!state.m_bInside &&
				!IsCombatLocked(state)
			)
			{
				m_aStates.Remove(i);
			}
		}
	}


	// =============================================================================================
	// INTRUDER
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected void StartIntruderCountdown(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;
	
	
		state.m_iIntruderStartTime =
			System.GetTickCount();
	
	
		state.m_iLastIntruderSecond =
			m_iEnemyKillDelaySeconds;
	
	
		ShowMessageParam1(
			state.m_Character,
			m_sEnemyWarningTitle,
			m_sEnemyWarningMessage,
			m_iEnemyKillDelaySeconds,
			BPS_COUNTDOWN_HINT_DURATION
		);
	
	
		DebugLog(
			string.Format(
				"INTRUDER START | Entity=%1 | Seconds=%2",
				state.m_Character,
				m_iEnemyKillDelaySeconds
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ProcessIntruder(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;
	
		IEntity character = state.m_Character;
	
		if (!character)
		{
			RemoveState(state);
			return;
		}
	
	
		// =========================================================================
		// PRESENCE
		//
		// Do NOT perform another geometry query here.
		//
		// OnActivate  -> m_bInside = true
		// OnDeactivate -> m_bInside = false
		// =========================================================================
	
		if (!state.m_bInside)
		{
			return;
		}
	
	
		// =========================================================================
		// FACTION CHANGED
		// =========================================================================
	
		if (IsProtectedFaction(character))
		{
			DebugLog(
				string.Format(
					"INTRUDER BECAME FRIENDLY | Entity=%1",
					character
				)
			);
	
			ResetIntruder(state);
	
			InitializeWeaponTracking(state);
	
			return;
		}
	
	
		// =========================================================================
		// COUNTDOWN
		// =========================================================================
	
		int elapsedMs =
			System.GetTickCount(
				state.m_iIntruderStartTime
			);
	
	
		int totalMs =
			m_iEnemyKillDelaySeconds *
			1000;
	
	
		int remainingMs =
			totalMs -
			elapsedMs;
	
	
		// =========================================================================
		// KILL
		// =========================================================================
	
		if (remainingMs <= 0)
		{
			DebugLog(
				string.Format(
					"INTRUDER COUNTDOWN FINISHED | Entity=%1",
					character
				)
			);
	
			KillIntruder(state);
	
			return;
		}
	
	
		// =========================================================================
		// UI COUNTDOWN
		// =========================================================================
	
		int remainingSeconds =
			(remainingMs + 999) /
			1000;
	
	
		if (
			remainingSeconds ==
			state.m_iLastIntruderSecond
		)
		{
			return;
		}
	
	
		state.m_iLastIntruderSecond =
			remainingSeconds;
	
	
		ShowMessageParam1(
			character,
			m_sEnemyWarningTitle,
			m_sEnemyWarningMessage,
			remainingSeconds,
			BPS_COUNTDOWN_HINT_DURATION
		);
	
	
		DebugLog(
			string.Format(
				"INTRUDER COUNTDOWN | Entity=%1 | Remaining=%2",
				character,
				remainingSeconds
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void KillIntruder(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;
	
	
		IEntity character =
			state.m_Character;
	
	
		if (!character)
		{
			RemoveState(state);
			return;
		}
	
	
		// =========================================================================
		// Only the trigger controls presence.
		// =========================================================================
	
		if (!state.m_bInside)
			return;
	
	
		// Faction changed before execution.
		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);
			return;
		}
	
	
		SCR_DamageManagerComponent damageManager =
			SCR_DamageManagerComponent.GetDamageManager(
				character
			);
	
	
		if (!damageManager)
		{
			DebugLog(
				string.Format(
					"KILL FAILED | No DamageManager | Entity=%1",
					character
				)
			);
	
			return;
		}
	
	
		if (damageManager.IsDestroyed())
		{
			ResetIntruder(state);
			RemoveState(state);
	
			return;
		}
	
	
		DebugLog(
			string.Format(
				"KILLING INTRUDER | Entity=%1 | Health=%2",
				character,
				damageManager.GetHealthScaled()
			)
		);
	
	
		ref Instigator instigator =
			Instigator.CreateInstigatorGM();
	
	
		if (instigator)
		{
			damageManager.Kill(
				instigator
			);
		}
	
	
		// Fallback
		if (!damageManager.IsDestroyed())
		{
			damageManager.SetHealthScaled(
				0
			);
		}
	
	
		DebugLog(
			string.Format(
				"KILL RESULT | Entity=%1 | Health=%2 | Destroyed=%3",
				character,
				damageManager.GetHealthScaled(),
				damageManager.IsDestroyed()
			)
		);
	
	
		if (
			damageManager.IsDestroyed() ||
			damageManager.GetHealthScaled() <= 0
		)
		{
			ResetIntruder(state);
	
			RemoveState(state);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsIntruderActive(
		BPS_CharacterState state
	)
	{
		if (!state)
			return false;


		return state.m_iIntruderStartTime >= 0;
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetIntruder(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		state.m_iIntruderStartTime =
			-1;


		state.m_iLastIntruderSecond =
			-1;
	}


	// =============================================================================================
	// COMBAT LOCK
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected void ApplyCombatLock(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		if (m_fCombatLockSeconds <= 0)
			return;


		bool alreadyLocked =
			IsCombatLocked(state);


		state.m_iCombatLockStartTime =
			System.GetTickCount();


		if (!alreadyLocked)
		{
			ShowMessage(
				state.m_Character,
				m_sCombatLockTitle,
				m_sCombatLockMessage
			);
		}


		DebugLog(
			string.Format(
				"COMBAT LOCK | Entity=%1",
				state.m_Character
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected bool IsCombatLocked(
		BPS_CharacterState state
	)
	{
		if (!state)
			return false;


		if (state.m_iCombatLockStartTime < 0)
			return false;


		int durationMs =
			m_fCombatLockSeconds *
			1000;


		return (
			System.GetTickCount(
				state.m_iCombatLockStartTime
			)
			<
			durationMs
		);
	}


	// =============================================================================================
	// WEAPON TRACKING
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected bool HasFired(
		BPS_CharacterState state
	)
	{
		if (!state)
			return false;


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
			ResetWeaponTracking(state);

			return false;
		}


		BaseMuzzleComponent muzzle =
			weapon.GetCurrentMuzzle();


		if (!muzzle)
		{
			ResetWeaponTracking(state);

			return false;
		}


		int ammo =
			muzzle.GetAmmoCount();


		// Weapon or muzzle changed.
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
	protected void InitializeWeaponTracking(
		BPS_CharacterState state
	)
	{
		ResetWeaponTracking(state);

		HasFired(state);
	}


	//------------------------------------------------------------------------------------------------
	protected void ResetWeaponTracking(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		state.m_Weapon = null;
		state.m_Muzzle = null;

		state.m_iLastAmmo = -1;

		state.m_bWeaponInitialized = false;
	}


	// =============================================================================================
	// DAMAGE
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	// Called from BPS_DamageManager.c
	//
	// TRUE = completely block damage.
	//------------------------------------------------------------------------------------------------
	static bool BPS_ShouldBlockDamage(
		IEntity victim,
		notnull BaseDamageContext damageContext
	)
	{
		if (!victim)
			return false;


		// Healing must continue normally.
		if (
			damageContext.damageType ==
				EDamageType.HEALING ||
			damageContext.damageType ==
				EDamageType.REGENERATION
		)
		{
			return false;
		}


		foreach (
			BPS_SafeZoneTriggerEntity zone :
			s_aZones
		)
		{
			if (!zone)
				continue;


			if (
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
		// Character-only core.
		if (!ChimeraCharacter.Cast(victim))
			return false;


		// Only our protected faction gets Safe Zone protection.
		if (!IsProtectedFaction(victim))
			return false;


		IEntity attacker = null;


		if (damageContext.instigator)
		{
			attacker =
				damageContext.instigator.GetInstigatorEntity();
		}


		// =========================================================================================
		// FRIENDLY FIRE
		//
		// Always blocked inside the Safe Zone,
		// EVEN when the victim is Combat Locked.
		// =========================================================================================

		if (
			attacker &&
			attacker != victim &&
			IsProtectedFaction(attacker)
		)
		{
			ShowMessage(
				attacker,
				m_sFriendlyFireTitle,
				m_sFriendlyFireMessage
			);


			return true;
		}


		// =========================================================================================
		// COMBAT LOCK
		//
		// Friendly fire has already been handled above.
		// During Combat Lock, enemy/environment damage is allowed.
		// =========================================================================================

		BPS_CharacterState victimState =
			FindState(victim);


		if (
			victimState &&
			IsCombatLocked(victimState)
		)
		{
			return false;
		}


		// =========================================================================================
		// NORMAL SAFE ZONE PROTECTION
		// =========================================================================================

		return true;
	}


	// =============================================================================================
	// FACTION
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected FactionKey GetFactionKey(
		IEntity ent
	)
	{
		if (!ent)
			return "";


		Faction faction =
			SCR_Faction.GetEntityFaction(
				ent
			);


		if (!faction)
			return "";


		return faction.GetFactionKey();
	}


	//------------------------------------------------------------------------------------------------
	// Intentionally exact FactionKey comparison.
	//
	// This keeps US / USSR behaviour deterministic while we stabilize BPS.
	//------------------------------------------------------------------------------------------------
	protected bool IsProtectedFaction(
		IEntity ent
	)
	{
		return (
			GetFactionKey(ent) ==
			m_sProtectedFactionKey
		);
	}


	// =============================================================================================
	// STATE
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected BPS_CharacterState GetOrCreateState(
		IEntity character
	)
	{
		BPS_CharacterState state =
			FindState(character);


		if (state)
			return state;


		state =
			new BPS_CharacterState(
				character
			);


		m_aStates.Insert(
			state
		);


		return state;
	}


	//------------------------------------------------------------------------------------------------
	protected BPS_CharacterState FindState(
		IEntity character
	)
	{
		if (!character)
			return null;


		foreach (
			BPS_CharacterState state :
			m_aStates
		)
		{
			if (
				state &&
				state.m_Character ==
					character
			)
			{
				return state;
			}
		}


		return null;
	}


	//------------------------------------------------------------------------------------------------
	protected void RemoveState(
		BPS_CharacterState state
	)
	{
		if (!state)
			return;


		int index =
			m_aStates.Find(
				state
			);


		if (index >= 0)
			m_aStates.Remove(index);
	}


	// =============================================================================================
	// UI
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
	protected SCR_PlayerController GetController(
		IEntity character
	)
	{
		if (!character)
			return null;


		PlayerManager playerManager =
			GetGame().GetPlayerManager();


		if (!playerManager)
			return null;


		int playerId =
			playerManager.GetPlayerIdFromControlledEntity(
				character
			);


		if (playerId <= 0)
			return null;


		return SCR_PlayerController.Cast(
			playerManager.GetPlayerController(
				playerId
			)
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ShowMessage(
		IEntity character,
		string title,
		string message
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
			m_fMessageDuration
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void ShowMessageParam1(
		IEntity character,
		string title,
		string message,
		int param1,
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

	//------------------------------------------------------------------------------------------------
	protected void DebugLog(
		string message
	)
	{
		if (!m_bDebug)
			return;


		PrintFormat(
			"[BPS] %1",
			message
		);
	}


	// =============================================================================================
	// CLEANUP
	// =============================================================================================

	//------------------------------------------------------------------------------------------------
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


		int zoneIndex =
			s_aZones.Find(
				this
			);


		if (zoneIndex >= 0)
			s_aZones.Remove(zoneIndex);
	}
}