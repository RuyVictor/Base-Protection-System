//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
// Safe Zone geometry: CYLINDER or SQUARE.
//
// Runtime gameplay checks use explicit mathematical volumes. Presence is refreshed from
// PlayerManager + AIWorld, never from QueryEntitiesBySphere. This keeps bot support without
// reintroducing a world-entity scan.
//------------------------------------------------------------------------------------------------
enum BPS_ETriggerShapeType
{
	Cylindrical = 0,
	Square = 1
}

//------------------------------------------------------------------------------------------------
enum BPS_EVehicleRelation
{
	Unknown = 0,
	Friendly = 1,
	Enemy = 2
}

//------------------------------------------------------------------------------------------------
class BPS_CharacterState
{
	IEntity m_Character;
	bool m_bInside;
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
		m_bInside = false;
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
		return m_bInside;
	}
}

//------------------------------------------------------------------------------------------------
class BPS_VehicleState
{
	IEntity m_Vehicle;
	bool m_bInside;
	bool m_bSeenThisPresencePass;
	BPS_EVehicleRelation m_eRelation;
	int m_iIntruderStartTime;
	int m_iLastIntruderSecond;

	void BPS_VehicleState(IEntity vehicle)
	{
		m_Vehicle = vehicle;
		m_bInside = false;
		m_bSeenThisPresencePass = false;
		m_eRelation = BPS_EVehicleRelation.Unknown;
		m_iIntruderStartTime = -1;
		m_iLastIntruderSecond = -1;
	}

	bool IsIntruderActive()
	{
		return m_iIntruderStartTime >= 0;
	}

	void ResetIntruder()
	{
		m_iIntruderStartTime = -1;
		m_iLastIntruderSecond = -1;
	}
}

//------------------------------------------------------------------------------------------------
class BPS_TurretWeaponState
{
	TurretControllerComponent m_Turret;
	IEntity m_Operator;
	BaseWeaponComponent m_Weapon;
	BaseMuzzleComponent m_Muzzle;
	int m_iLastAmmo;
	bool m_bInitialized;
	bool m_bSeen;

	void BPS_TurretWeaponState(TurretControllerComponent turret)
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
	description: "Base Protection System Safe Zone. Supports cylindrical and square volumes."
)]
class BPS_SafeZoneTriggerEntityClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class BPS_SafeZoneTriggerEntity : GenericEntity
{
	protected static const int BPS_LOGIC_INTERVAL_MS = 100;
	protected static const int BPS_PRESENCE_INTERVAL_MS = 500;
	protected static const int BPS_FACTION_CHECK_INTERVAL_MS = 1000;
	protected static const int BPS_MAP_CYLINDER_SEGMENTS = 32;

	// =============================================================================================
	// SHAPE
	// =============================================================================================
	[Attribute(
		"0",
		UIWidgets.ComboBox,
		"Trigger Shape Type",
		"",
		ParamEnumArray.FromEnum(BPS_ETriggerShapeType)
	)]
	protected BPS_ETriggerShapeType m_eTriggerShapeType;

	[Attribute(
		"200",
		UIWidgets.Slider,
		"Horizontal Size. Cylindrical = diameter. Square = side length.",
		"1 10000 1"
	)]
	protected float m_fHorizontalSize;

	[Attribute(
		"40",
		UIWidgets.Slider,
		"Safe Zone height. The volume extends half above and half below the entity origin.",
		"1 10000 1"
	)]
	protected float m_fHeight;

	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show the real BPS volume in the World Editor and in-game."
	)]
	protected bool m_bShowDebugShape;

	// =============================================================================================
	// CONFIGURATION
	// =============================================================================================
	[Attribute("", UIWidgets.Object, "Friendly enter/exit UI.")]
	ref BPS_FriendlyMessagesConfig m_FriendlyMessages;

	[Attribute("", UIWidgets.Object, "Enemy intrusion configuration.")]
	ref BPS_IntruderConfig m_IntruderConfig;

	[Attribute("", UIWidgets.Object, "Combat Lock configuration.")]
	ref BPS_CombatConfig m_CombatConfig;

	[Attribute("", UIWidgets.Object, "Friendly fire configuration.")]
	ref BPS_FriendlyFireConfig m_FriendlyFireConfig;

	[Attribute("", UIWidgets.Object, "Map Safe Zone boundary configuration.")]
	ref BPS_MapDisplayConfig m_MapDisplayConfig;

	// =============================================================================================
	// CAMPAIGN / REGISTRIES / RUNTIME STATE
	// =============================================================================================
	protected SCR_CampaignMilitaryBaseComponent m_CampaignBase;
	protected FactionKey m_sLastBaseFactionKey;
	protected bool m_bBPSInitialized;
	protected int m_iLastPresenceRefreshTime = -1;
	protected int m_iLastFactionCheckTime = -1;

	protected static ref array<BPS_SafeZoneTriggerEntity> s_aZones = new array<BPS_SafeZoneTriggerEntity>();
	protected static ref array<BPS_SafeZoneTriggerEntity> s_aMapZones = new array<BPS_SafeZoneTriggerEntity>();

	protected ref array<ref BPS_CharacterState> m_aStates = new array<ref BPS_CharacterState>();
	protected ref array<ref BPS_VehicleState> m_aVehicleStates = new array<ref BPS_VehicleState>();
	protected ref array<ref BPS_TurretWeaponState> m_aTurretStates = new array<ref BPS_TurretWeaponState>();

	protected ref array<int> m_aPlayerIds = new array<int>();
	protected ref array<AIAgent> m_aAIAgents = new array<AIAgent>();
	protected ref array<IEntity> m_aVehicleOccupants = new array<IEntity>();

	// =============================================================================================
	// INIT
	// =============================================================================================
	void BPS_SafeZoneTriggerEntity(IEntitySource src, IEntity parent)
	{
		SetFlags(EntityFlags.ACTIVE);
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		EnsureConfig();
		BPS_CreateRuntimeDebugShape();

		if (s_aMapZones.Find(this) < 0)
			s_aMapZones.Insert(this);

		if (Replication.IsRunning() && !Replication.IsServer())
		{
			if (GetGame() && GetGame().GetCallqueue())
			{
				GetGame().GetCallqueue().CallLater(
					BPS_TryBindCampaignBaseForMap,
					500,
					true
				);
			}

			return;
		}

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(
				BPS_TryInitialize,
				500,
				true
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureConfig()
	{
		if (!m_FriendlyMessages)
			m_FriendlyMessages = new BPS_FriendlyMessagesConfig();

		if (!m_IntruderConfig)
			m_IntruderConfig = new BPS_IntruderConfig();

		if (!m_CombatConfig)
			m_CombatConfig = new BPS_CombatConfig();

		if (!m_FriendlyFireConfig)
			m_FriendlyFireConfig = new BPS_FriendlyFireConfig();

		if (!m_MapDisplayConfig)
			m_MapDisplayConfig = new BPS_MapDisplayConfig();
	}

	// =============================================================================================
	// EXACT SHAPE
	// =============================================================================================
	protected float BPS_GetHorizontalSize()
	{
		if (m_fHorizontalSize < 1.0)
			return 1.0;

		return m_fHorizontalSize;
	}

	//------------------------------------------------------------------------------------------------
	protected float BPS_GetHeight()
	{
		if (m_fHeight < 1.0)
			return 1.0;

		return m_fHeight;
	}

	//------------------------------------------------------------------------------------------------
	bool BPS_IsWorldPositionInsideShape(vector worldPosition)
	{
		vector transform[4];
		GetWorldTransform(transform);
		vector localPosition = worldPosition.InvMultiply4(transform);

		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		float halfHeight = BPS_GetHeight() * 0.5;

		if (Math.AbsFloat(localPosition[1]) > halfHeight)
			return false;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
		{
			return (
				Math.AbsFloat(localPosition[0]) <= halfHorizontal &&
				Math.AbsFloat(localPosition[2]) <= halfHorizontal
			);
		}

		float distanceSq =
			localPosition[0] * localPosition[0] +
			localPosition[2] * localPosition[2];

		return distanceSq <= halfHorizontal * halfHorizontal;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_IsEntityInsideShape(IEntity ent)
	{
		if (!ent)
			return false;

		return BPS_IsWorldPositionInsideShape(ent.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_IsCharacterInsideShape(ChimeraCharacter character)
	{
		if (!character)
			return false;

		if (BPS_IsEntityInsideShape(character))
			return true;

		IEntity vehicle = BPS_GetCharacterVehicle(character);
		if (!vehicle)
			return false;

		return BPS_IsEntityInsideShape(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity BPS_GetCharacterVehicle(ChimeraCharacter character)
	{
		if (!character)
			return null;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !access.IsInCompartment())
			return null;

		return CompartmentAccessComponent.GetVehicleIn(character);
	}

	// =============================================================================================
	// CLIENT MAP BASE BINDING
	// =============================================================================================
	protected void BPS_TryBindCampaignBaseForMap()
	{
		if (m_CampaignBase)
		{
			StopMapBindingTimer();
			return;
		}

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign)
			return;

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (!baseManager || !baseManager.IsBasesInitDone())
			return;

		m_CampaignBase = baseManager.FindClosestBase(GetOrigin());
		if (!m_CampaignBase)
			return;

		StopMapBindingTimer();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopMapBindingTimer()
	{
		if (!GetGame())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(BPS_TryBindCampaignBaseForMap);
	}

	// =============================================================================================
	// PRESENCE - HUMAN PLAYERS + AI, NO WORLD QUERY
	// =============================================================================================
	protected void BPS_RefreshPresence()
	{
		foreach (BPS_CharacterState state : m_aStates)
		{
			if (state)
				state.m_bInside = false;
		}

		foreach (BPS_VehicleState vehicleState : m_aVehicleStates)
		{
			if (vehicleState)
				vehicleState.m_bSeenThisPresencePass = false;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			m_aPlayerIds.Clear();
			playerManager.GetPlayers(m_aPlayerIds);

			foreach (int playerId : m_aPlayerIds)
			{
				IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
				ChimeraCharacter character = ChimeraCharacter.Cast(controlled);
				if (!character)
					continue;

				BPS_ProcessPresenceCharacter(character);
			}
		}

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
		{
			m_aAIAgents.Clear();
			aiWorld.GetAIAgents(m_aAIAgents);

			foreach (AIAgent agent : m_aAIAgents)
			{
				if (!agent)
					continue;

				ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!character)
					continue;

				BPS_ProcessPresenceCharacter(character);
			}
		}

		BPS_RefreshTrackedVehiclePresence();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_ProcessPresenceCharacter(ChimeraCharacter character)
	{
		if (!character)
			return;

		if (BPS_IsCharacterInsideShape(character))
		{
			BPS_CharacterState state = GetOrCreateCharacterState(character);
			state.m_bInside = true;
		}

		IEntity vehicle = BPS_GetCharacterVehicle(character);
		if (!vehicle)
			return;

		BPS_VehicleState vehicleState = FindVehicleState(vehicle);
		bool vehicleInside = BPS_IsEntityInsideShape(vehicle);

		if (!vehicleState && !vehicleInside)
			return;

		if (!vehicleState)
			vehicleState = GetOrCreateVehicleState(vehicle);

		if (vehicleState.m_bSeenThisPresencePass)
			return;

		vehicleState.m_bSeenThisPresencePass = true;
		vehicleState.m_bInside = vehicleInside;
		vehicleState.m_eRelation = BPS_GetVehicleRelation(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_RefreshTrackedVehiclePresence()
	{
		for (int i = m_aVehicleStates.Count() - 1; i >= 0; i--)
		{
			BPS_VehicleState state = m_aVehicleStates[i];
			if (!state || !state.m_Vehicle)
			{
				m_aVehicleStates.Remove(i);
				continue;
			}

			SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.GetDamageManager(state.m_Vehicle);
			if (damageManager && damageManager.IsDestroyed())
			{
				m_aVehicleStates.Remove(i);
				continue;
			}

			state.m_bInside = BPS_IsEntityInsideShape(state.m_Vehicle);
			state.m_eRelation = BPS_GetVehicleRelation(state.m_Vehicle);
		}
	}

	// =============================================================================================
	// LOGIC LOOP
	// =============================================================================================
	protected void BPS_LogicTick()
	{
		if (
			m_iLastFactionCheckTime < 0 ||
			System.GetTickCount(m_iLastFactionCheckTime) >= BPS_FACTION_CHECK_INTERVAL_MS
		)
		{
			BPS_CheckBaseFaction();
			m_iLastFactionCheckTime = System.GetTickCount();
		}

		if (
			m_iLastPresenceRefreshTime < 0 ||
			System.GetTickCount(m_iLastPresenceRefreshTime) >= BPS_PRESENCE_INTERVAL_MS
		)
		{
			BPS_RefreshPresence();
			m_iLastPresenceRefreshTime = System.GetTickCount();
		}

		foreach (BPS_TurretWeaponState turretState : m_aTurretStates)
		{
			if (turretState)
				turretState.m_bSeen = false;
		}

		// Vehicle intruders are processed before characters so the vehicle destruction is the
		// authoritative result when both timers expire on the same tick.
		ProcessVehicles();
		ProcessCharacters();
		CleanupTurretStates();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_CheckBaseFaction()
	{
		if (!m_CampaignBase)
			return;

		Faction faction = m_CampaignBase.GetFaction();
		if (!faction)
			return;

		FactionKey currentFactionKey = faction.GetFactionKey();
		if (currentFactionKey == m_sLastBaseFactionKey)
			return;

		FactionKey previousFactionKey = m_sLastBaseFactionKey;
		m_sLastBaseFactionKey = currentFactionKey;

		PrintFormat(
			"[BPS] BASE FACTION CHANGED | Base=%1 | %2 -> %3",
			m_CampaignBase.GetBaseName(),
			previousFactionKey,
			currentFactionKey
		);

		foreach (BPS_CharacterState state : m_aStates)
		{
			if (!state)
				continue;

			ResetIntruder(state);
			state.m_iCombatLockStartTime = -1;
			state.m_bWasInside = false;
		}

		foreach (BPS_VehicleState vehicleState : m_aVehicleStates)
		{
			if (!vehicleState)
				continue;

			vehicleState.ResetIntruder();
			vehicleState.m_eRelation = BPS_GetVehicleRelation(vehicleState.m_Vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_TryInitialize()
	{
		if (m_bBPSInitialized)
			return;

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign)
			return;

		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		if (!baseManager || !baseManager.IsBasesInitDone())
			return;

		m_CampaignBase = baseManager.FindClosestBase(GetOrigin());
		if (!m_CampaignBase)
		{
			Print("[BPS] Could not find Campaign base near Safe Zone.", LogLevel.ERROR);
			StopInitializationTimer();
			return;
		}

		if (!m_CampaignBase.IsHQ())
		{
			StopInitializationTimer();
			m_bBPSInitialized = true;
			return;
		}

		Faction faction = m_CampaignBase.GetFaction();
		if (!faction)
			return;

		m_sLastBaseFactionKey = faction.GetFactionKey();
		m_bBPSInitialized = true;
		StopInitializationTimer();

		if (s_aZones.Find(this) < 0)
			s_aZones.Insert(this);

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(
				BPS_LogicTick,
				BPS_LOGIC_INTERVAL_MS,
				true
			);
		}

		PrintFormat(
			"[BPS] ACTIVATED | Base=%1 | Faction=%2 | Shape=%3 | HorizontalSize=%4 | Height=%5",
			m_CampaignBase.GetBaseName(),
			m_sLastBaseFactionKey,
			m_eTriggerShapeType,
			BPS_GetHorizontalSize(),
			BPS_GetHeight()
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopInitializationTimer()
	{
		if (!GetGame())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(BPS_TryInitialize);
	}

	//------------------------------------------------------------------------------------------------
	protected Faction GetProtectedFaction()
	{
		if (!m_bBPSInitialized || !m_CampaignBase || !m_CampaignBase.IsHQ())
			return null;

		return m_CampaignBase.GetFaction();
	}

	// =============================================================================================
	// VEHICLE RELATION / STATE
	// =============================================================================================
	protected SCR_BaseCompartmentManagerComponent BPS_GetVehicleCompartmentManager(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		IEntity current = vehicle;
		for (int depth = 0; current && depth < 4; depth++)
		{
			SCR_BaseCompartmentManagerComponent manager = SCR_BaseCompartmentManagerComponent.Cast(
				current.FindComponent(SCR_BaseCompartmentManagerComponent)
			);

			if (manager)
				return manager;

			current = current.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_IsFactionProtected(Faction faction)
	{
		if (!faction)
			return false;

		Faction protectedFaction = GetProtectedFaction();
		if (!protectedFaction)
			return false;

		if (faction.GetFactionKey() == protectedFaction.GetFactionKey())
			return true;

		return protectedFaction.IsFactionFriendly(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_IsFactionEnemy(Faction faction)
	{
		if (!faction)
			return false;

		Faction protectedFaction = GetProtectedFaction();
		if (!protectedFaction)
			return false;

		if (faction.GetFactionKey() == protectedFaction.GetFactionKey())
			return false;

		return protectedFaction.IsFactionEnemy(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_EVehicleRelation BPS_GetVehicleRelation(IEntity vehicle)
	{
		if (!vehicle)
			return BPS_EVehicleRelation.Unknown;

		bool hasFriendlyOccupant = false;
		bool hasEnemyOccupant = false;

		SCR_BaseCompartmentManagerComponent compartmentManager = BPS_GetVehicleCompartmentManager(vehicle);
		if (compartmentManager)
		{
			m_aVehicleOccupants.Clear();
			compartmentManager.GetOccupants(m_aVehicleOccupants);

			foreach (IEntity occupant : m_aVehicleOccupants)
			{
				if (!occupant)
					continue;

				Faction occupantFaction = GetEntityFactionForBPS(occupant);
				if (!occupantFaction)
					continue;

				if (BPS_IsFactionProtected(occupantFaction))
				{
					hasFriendlyOccupant = true;
					continue;
				}

				if (BPS_IsFactionEnemy(occupantFaction))
					hasEnemyOccupant = true;
			}
		}

		// Safety first: never destroy a vehicle while a protected/friendly occupant is inside.
		if (hasFriendlyOccupant)
			return BPS_EVehicleRelation.Friendly;

		if (hasEnemyOccupant)
			return BPS_EVehicleRelation.Enemy;

		// Empty vehicle (or a prefab without accessible occupants): use its faction affiliation.
		Faction vehicleFaction = GetEntityFactionForBPS(vehicle);
		if (!vehicleFaction)
			return BPS_EVehicleRelation.Unknown;

		if (BPS_IsFactionProtected(vehicleFaction))
			return BPS_EVehicleRelation.Friendly;

		if (BPS_IsFactionEnemy(vehicleFaction))
			return BPS_EVehicleRelation.Enemy;

		return BPS_EVehicleRelation.Unknown;
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_VehicleState GetOrCreateVehicleState(IEntity vehicle)
	{
		BPS_VehicleState existing = FindVehicleState(vehicle);
		if (existing)
			return existing;

		BPS_VehicleState state = new BPS_VehicleState(vehicle);
		m_aVehicleStates.Insert(state);
		return state;
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_VehicleState FindVehicleState(IEntity vehicle)
	{
		foreach (BPS_VehicleState state : m_aVehicleStates)
		{
			if (state && state.m_Vehicle == vehicle)
				return state;
		}

		return null;
	}

	// =============================================================================================
	// ENEMY VEHICLE INTRUSION - ONE COUNTDOWN PER VEHICLE
	// =============================================================================================
	protected void ProcessVehicles()
	{
		for (int i = m_aVehicleStates.Count() - 1; i >= 0; i--)
		{
			BPS_VehicleState state = m_aVehicleStates[i];
			if (!state || !state.m_Vehicle)
			{
				m_aVehicleStates.Remove(i);
				continue;
			}

			SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.GetDamageManager(state.m_Vehicle);
			if (damageManager && damageManager.IsDestroyed())
			{
				m_aVehicleStates.Remove(i);
				continue;
			}

			// Exact geometry is reconciled every logic tick. Presence discovery/faction classification
			// stays at 500 ms. Only an ACTIVE enemy countdown re-checks occupant relation at 100 ms,
			// avoiding repeated compartment scans for every friendly vehicle in the zone.
			state.m_bInside = BPS_IsEntityInsideShape(state.m_Vehicle);
			if (state.IsIntruderActive())
				state.m_eRelation = BPS_GetVehicleRelation(state.m_Vehicle);

			if (!state.m_bInside)
			{
				if (state.IsIntruderActive())
				{
					PrintFormat("[BPS] ENEMY VEHICLE LEFT SAFE ZONE - COUNTDOWN CANCELLED | Vehicle=%1", state.m_Vehicle);
					state.ResetIntruder();
				}

				// Keep nothing for an unoccupied/unseen vehicle once it is outside.
				if (!state.m_bSeenThisPresencePass)
					m_aVehicleStates.Remove(i);

				continue;
			}

			if (state.m_eRelation != BPS_EVehicleRelation.Enemy)
			{
				if (state.IsIntruderActive())
				{
					PrintFormat("[BPS] VEHICLE IS NO LONGER ENEMY - COUNTDOWN CANCELLED | Vehicle=%1", state.m_Vehicle);
					state.ResetIntruder();
				}

				// Friendly/neutral empty vehicles do not need a persistent timer state. Vehicle
				// damage protection is evaluated directly when damage arrives.
				if (!state.m_bSeenThisPresencePass)
					m_aVehicleStates.Remove(i);

				continue;
			}

			if (!state.IsIntruderActive())
				StartVehicleIntruder(state);

			ProcessVehicleIntruder(state);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StartVehicleIntruder(BPS_VehicleState state)
	{
		if (!state || !state.m_Vehicle)
			return;

		state.m_iIntruderStartTime = System.GetTickCount();
		state.m_iLastIntruderSecond = GetIntruderDelay();

		PrintFormat(
			"[BPS] ENEMY VEHICLE ENTERED SAFE ZONE | Vehicle=%1 | Countdown=%2s",
			state.m_Vehicle,
			GetIntruderDelay()
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessVehicleIntruder(BPS_VehicleState state)
	{
		if (!state || !state.m_Vehicle || !state.IsIntruderActive())
			return;

		// Validate both conditions again at the precise timer tick.
		if (!BPS_IsEntityInsideShape(state.m_Vehicle))
		{
			state.ResetIntruder();
			return;
		}

		if (BPS_GetVehicleRelation(state.m_Vehicle) != BPS_EVehicleRelation.Enemy)
		{
			state.ResetIntruder();
			return;
		}

		int elapsed = System.GetTickCount(state.m_iIntruderStartTime);
		int total = GetIntruderDelay() * 1000;
		int remainingMs = total - elapsed;

		if (remainingMs <= 0)
		{
			DestroyIntruderVehicle(state);
			return;
		}

		int remaining = (remainingMs + 999) / 1000;
		if (remaining == state.m_iLastIntruderSecond)
			return;

		state.m_iLastIntruderSecond = remaining;
		PrintFormat(
			"[BPS] ENEMY VEHICLE COUNTDOWN | Vehicle=%1 | Remaining=%2s",
			state.m_Vehicle,
			remaining
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyIntruderVehicle(BPS_VehicleState state)
	{
		if (!state || !state.m_Vehicle)
			return;

		IEntity vehicle = state.m_Vehicle;

		// Last chance checks: leaving/capture on the same tick must cancel destruction.
		if (!BPS_IsEntityInsideShape(vehicle))
		{
			state.ResetIntruder();
			return;
		}

		if (BPS_GetVehicleRelation(vehicle) != BPS_EVehicleRelation.Enemy)
		{
			state.ResetIntruder();
			return;
		}

		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!manager)
		{
			PrintFormat("[BPS] Cannot destroy enemy vehicle: no DamageManager | Vehicle=%1", vehicle);
			state.ResetIntruder();
			return;
		}

		if (manager.IsDestroyed())
		{
			state.ResetIntruder();
			return;
		}

		PrintFormat("[BPS] ENEMY VEHICLE COUNTDOWN EXPIRED - DESTROYING | Vehicle=%1", vehicle);

		ref Instigator instigator = Instigator.CreateInstigatorGM();
		if (instigator)
			manager.Kill(instigator);

		// Fallback for unusual/modded damage managers that do not transition on Kill().
		if (!manager.IsDestroyed())
			manager.SetHealthScaled(0);

		state.ResetIntruder();
	}

	// =============================================================================================
	// TURRET / MOUNTED WEAPON - CURRENT OCCUPIED SEAT ONLY
	// =============================================================================================
	protected bool ProcessCharacterTurret(BPS_CharacterState characterState)
	{
		if (!characterState || !characterState.m_Character)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(characterState.m_Character);
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !access.IsInCompartment())
			return false;

		ExtBaseCompartmentSlot slot = ExtBaseCompartmentSlot.Cast(access.GetCompartment());
		if (!slot)
			return false;

		TurretControllerComponent turret = slot.GetAttachedTurret();
		if (!turret)
			return false;

		BPS_TurretWeaponState turretState = GetOrCreateTurretState(turret);
		turretState.m_bSeen = true;

		if (turretState.m_Operator != character)
		{
			turretState.m_Operator = character;
			ResetTurretWeaponState(turretState);
		}

		if (GetCombatDuration() > 0 && HasTurretFired(turretState))
			ApplyCombatLock(characterState);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasTurretFired(BPS_TurretWeaponState state)
	{
		if (!state || !state.m_Turret)
			return false;

		BaseWeaponManagerComponent weaponManager = state.m_Turret.GetWeaponManager();
		if (!weaponManager)
		{
			ResetTurretWeaponState(state);
			return false;
		}

		BaseWeaponComponent weapon = SCR_WeaponLib.GetCurrentWeaponComponent(weaponManager);
		if (!weapon)
		{
			ResetTurretWeaponState(state);
			return false;
		}

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
		{
			ResetTurretWeaponState(state);
			return false;
		}

		int ammo = muzzle.GetAmmoCount();
		if (state.m_Weapon != weapon || state.m_Muzzle != muzzle)
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

		bool fired = ammo < state.m_iLastAmmo;
		state.m_iLastAmmo = ammo;
		return fired;
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetTurretWeaponState(BPS_TurretWeaponState state)
	{
		if (!state)
			return;

		state.m_Weapon = null;
		state.m_Muzzle = null;
		state.m_iLastAmmo = -1;
		state.m_bInitialized = false;
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_TurretWeaponState GetOrCreateTurretState(TurretControllerComponent turret)
	{
		foreach (BPS_TurretWeaponState state : m_aTurretStates)
		{
			if (state && state.m_Turret == turret)
				return state;
		}

		BPS_TurretWeaponState newState = new BPS_TurretWeaponState(turret);
		m_aTurretStates.Insert(newState);
		return newState;
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanupTurretStates()
	{
		for (int i = m_aTurretStates.Count() - 1; i >= 0; i--)
		{
			BPS_TurretWeaponState state = m_aTurretStates[i];
			if (!state || !state.m_bSeen)
				m_aTurretStates.Remove(i);
		}
	}

	// =============================================================================================
	// CHARACTERS
	// =============================================================================================
	protected void ProcessCharacters()
	{
		for (int i = m_aStates.Count() - 1; i >= 0; i--)
		{
			BPS_CharacterState state = m_aStates[i];
			if (!state || !state.m_Character)
			{
				m_aStates.Remove(i);
				continue;
			}

			ChimeraCharacter character = ChimeraCharacter.Cast(state.m_Character);
			if (!character)
			{
				m_aStates.Remove(i);
				continue;
			}

			// Reconcile on every logic tick so a character/vehicle exit cannot remain stale for 500 ms.
			if (state.m_bInside && !BPS_IsCharacterInsideShape(character))
				state.m_bInside = false;

			bool inside = state.IsInside();
			if (inside && !state.m_bWasInside)
			{
				state.m_bWasInside = true;
				OnCharacterEntered(state);
			}
			else if (!inside && state.m_bWasInside)
			{
				state.m_bWasInside = false;
				OnCharacterExited(state);
			}

			if (inside && IsIntruderActive(state))
			{
				ProcessIntruder(state);
				continue;
			}

			if (inside && IsProtectedFaction(state.m_Character))
			{
				bool hasControlledTurret = ProcessCharacterTurret(state);
				if (!hasControlledTurret && GetCombatDuration() > 0 && HasPersonalWeaponFired(state))
					ApplyCombatLock(state);

				ProcessCombatLockExpiration(state);
			}

			if (!inside && !IsIntruderActive(state) && !IsCombatLocked(state))
				m_aStates.Remove(i);
		}
	}

	// =============================================================================================
	// ENTER / EXIT
	// =============================================================================================
	protected void OnCharacterEntered(BPS_CharacterState state)
	{
		IEntity character = state.m_Character;
		if (IsProtectedFaction(character))
		{
			ResetIntruder(state);
			InitializePersonalWeaponTracking(state);

			if (m_FriendlyMessages.IsEnabled())
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

		StartIntruder(state);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCharacterExited(BPS_CharacterState state)
	{
		IEntity character = state.m_Character;
		ResetPersonalWeaponTracking(state);

		if (IsIntruderActive(state))
		{
			ResetIntruder(state);
			ShowMessage(
				character,
				m_IntruderConfig.GetExitTitle(),
				m_IntruderConfig.GetExitMessage(),
				m_IntruderConfig.GetExitMessageDuration()
			);
			return;
		}

		if (IsProtectedFaction(character) && m_FriendlyMessages.IsEnabled())
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
	// PERSONAL WEAPON / COMBAT LOCK
	// =============================================================================================
	protected bool HasPersonalWeaponFired(BPS_CharacterState state)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(state.m_Character);
		if (!character)
			return false;

		BaseWeaponComponent weapon = SCR_WeaponLib.GetCurrentWeaponComponent(character);
		if (!weapon)
		{
			ResetPersonalWeaponTracking(state);
			return false;
		}

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
		{
			ResetPersonalWeaponTracking(state);
			return false;
		}

		int ammo = muzzle.GetAmmoCount();
		if (state.m_Weapon != weapon || state.m_Muzzle != muzzle)
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

		bool fired = ammo < state.m_iLastAmmo;
		state.m_iLastAmmo = ammo;
		return fired;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitializePersonalWeaponTracking(BPS_CharacterState state)
	{
		ResetPersonalWeaponTracking(state);
		HasPersonalWeaponFired(state);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetPersonalWeaponTracking(BPS_CharacterState state)
	{
		if (!state)
			return;

		state.m_Weapon = null;
		state.m_Muzzle = null;
		state.m_iLastAmmo = -1;
		state.m_bWeaponInitialized = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCombatLock(BPS_CharacterState state)
	{
		if (!state || GetCombatDuration() <= 0)
			return;

		bool alreadyLocked = IsCombatLocked(state);
		state.m_iCombatLockStartTime = System.GetTickCount();

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
	protected bool IsCombatLocked(BPS_CharacterState state)
	{
		if (!state || state.m_iCombatLockStartTime < 0)
			return false;

		int durationMs = GetCombatDuration() * 1000;
		return System.GetTickCount(state.m_iCombatLockStartTime) < durationMs;
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessCombatLockExpiration(BPS_CharacterState state)
	{
		if (!state || state.m_iCombatLockStartTime < 0 || IsCombatLocked(state))
			return;

		state.m_iCombatLockStartTime = -1;
		ShowMessage(
			state.m_Character,
			m_CombatConfig.GetRestoredTitle(),
			m_CombatConfig.GetRestoredMessage(),
			m_CombatConfig.GetMessageDuration()
		);
	}

	// =============================================================================================
	// CHARACTER INTRUDER
	// =============================================================================================
	protected void StartIntruder(BPS_CharacterState state)
	{
		if (!state)
			return;

		state.m_iIntruderStartTime = System.GetTickCount();
		state.m_iLastIntruderSecond = GetIntruderDelay();
		ShowMessageParam1(
			state.m_Character,
			m_IntruderConfig.GetWarningTitle(),
			m_IntruderConfig.GetWarningMessage(),
			m_IntruderConfig.GetCountdownMessageDuration(),
			GetIntruderDelay()
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessIntruder(BPS_CharacterState state)
	{
		if (!state || !state.IsInside())
			return;

		if (IsProtectedFaction(state.m_Character))
		{
			ResetIntruder(state);
			return;
		}

		int elapsed = System.GetTickCount(state.m_iIntruderStartTime);
		int total = GetIntruderDelay() * 1000;
		int remainingMs = total - elapsed;

		if (remainingMs <= 0)
		{
			KillIntruder(state);
			return;
		}

		int remaining = (remainingMs + 999) / 1000;
		if (remaining == state.m_iLastIntruderSecond)
			return;

		state.m_iLastIntruderSecond = remaining;
		ShowMessageParam1(
			state.m_Character,
			m_IntruderConfig.GetWarningTitle(),
			m_IntruderConfig.GetWarningMessage(),
			m_IntruderConfig.GetCountdownMessageDuration(),
			remaining
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void KillIntruder(BPS_CharacterState state)
	{
		if (!state || !state.IsInside())
			return;

		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.GetDamageManager(state.m_Character);
		if (!manager)
			return;

		if (manager.IsDestroyed())
		{
			ResetIntruder(state);
			return;
		}

		ref Instigator instigator = Instigator.CreateInstigatorGM();
		if (instigator)
			manager.Kill(instigator);

		if (!manager.IsDestroyed())
			manager.SetHealthScaled(0);

		if (manager.IsDestroyed() || manager.GetHealthScaled() <= 0)
			ResetIntruder(state);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIntruderActive(BPS_CharacterState state)
	{
		return state && state.m_iIntruderStartTime >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetIntruder(BPS_CharacterState state)
	{
		if (!state)
			return;

		state.m_iIntruderStartTime = -1;
		state.m_iLastIntruderSecond = -1;
	}

	// =============================================================================================
	// DAMAGE
	// =============================================================================================
	static bool BPS_ShouldBlockDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (
			damageContext.damageType == EDamageType.HEALING ||
			damageContext.damageType == EDamageType.REGENERATION
		)
		{
			return false;
		}

		foreach (BPS_SafeZoneTriggerEntity zone : s_aZones)
		{
			if (zone && zone.ShouldBlockDamage(victim, damageContext))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!victim)
			return false;

		if (ChimeraCharacter.Cast(victim))
			return ShouldBlockCharacterDamage(victim, damageContext);

		if (Vehicle.Cast(victim))
			return ShouldBlockVehicleDamage(victim, damageContext);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockCharacterDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		ChimeraCharacter victimCharacter = ChimeraCharacter.Cast(victim);
		if (!victimCharacter)
			return false;

		if (!IsProtectedFaction(victim))
			return false;

		// Critical: damage protection uses exact geometry, not the 500 ms presence cache.
		// This protects bots as well as players and also handles occupants through vehicle root.
		if (!BPS_IsCharacterInsideShape(victimCharacter))
			return false;

		BPS_CharacterState victimState = FindCharacterState(victim);
		if (victimState && IsCombatLocked(victimState))
			return false;

		IEntity attacker = GetDamageInstigatorEntity(damageContext);
		if (
			m_FriendlyFireConfig.IsEnabled() &&
			attacker &&
			attacker != victim &&
			IsProtectedFaction(attacker)
		)
		{
			ChimeraCharacter attackerCharacter = ChimeraCharacter.Cast(attacker);
			if (attackerCharacter && BPS_IsCharacterInsideShape(attackerCharacter))
			{
				BPS_CharacterState attackerState = GetOrCreateCharacterState(attackerCharacter);
				attackerState.m_bInside = true;
				ShowFriendlyFireMessage(attackerCharacter, attackerState);
			}
		}

		// Protected/friendly character inside the safe zone receives no hostile damage.
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldBlockVehicleDamage(IEntity vehicle, notnull BaseDamageContext damageContext)
	{
		if (!vehicle || !BPS_IsEntityInsideShape(vehicle))
			return false;

		// Vehicle protection is independent of attacker resolution and independent of the FF warning
		// checkbox. The checkbox controls only the warning shown to a friendly attacker.
		if (BPS_GetVehicleRelation(vehicle) != BPS_EVehicleRelation.Friendly)
			return false;

		IEntity attacker = GetDamageInstigatorEntity(damageContext);
		if (
			m_FriendlyFireConfig.IsEnabled() &&
			attacker &&
			attacker != vehicle &&
			IsProtectedFaction(attacker)
		)
		{
			ChimeraCharacter attackerCharacter = ChimeraCharacter.Cast(attacker);
			if (attackerCharacter && BPS_IsCharacterInsideShape(attackerCharacter))
			{
				BPS_CharacterState attackerState = GetOrCreateCharacterState(attackerCharacter);
				attackerState.m_bInside = true;
				ShowFriendlyFireMessage(attackerCharacter, attackerState);
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetDamageInstigatorEntity(notnull BaseDamageContext damageContext)
	{
		if (!damageContext.instigator)
			return null;

		return damageContext.instigator.GetInstigatorEntity();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowFriendlyFireMessage(IEntity attacker, BPS_CharacterState state)
	{
		if (!attacker || !state)
			return;

		int cooldown = m_FriendlyFireConfig.GetWarningCooldown() * 1000;
		if (
			state.m_iFriendlyFireMessageTime >= 0 &&
			System.GetTickCount(state.m_iFriendlyFireMessageTime) < cooldown
		)
		{
			return;
		}

		state.m_iFriendlyFireMessageTime = System.GetTickCount();
		ShowMessage(
			attacker,
			m_FriendlyFireConfig.GetTitle(),
			m_FriendlyFireConfig.GetMessage(),
			m_FriendlyFireConfig.GetMessageDuration()
		);
	}

	// =============================================================================================
	// FACTION HELPERS
	// =============================================================================================
	protected Faction GetEntityFactionForBPS(IEntity ent)
	{
		if (!ent)
			return null;

		Faction faction = SCR_Faction.GetEntityFaction(ent);
		if (faction)
			return faction;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(
			ent.FindComponent(FactionAffiliationComponent)
		);

		if (!affiliation)
			return null;

		faction = affiliation.GetAffiliatedFaction();
		if (faction)
			return faction;

		return affiliation.GetDefaultAffiliatedFaction();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsProtectedFaction(IEntity ent)
	{
		if (!ent)
			return false;

		return BPS_IsFactionProtected(GetEntityFactionForBPS(ent));
	}

	// =============================================================================================
	// MAP API
	// =============================================================================================
	static void BPS_CopyMapZones(notnull array<BPS_SafeZoneTriggerEntity> zones)
	{
		zones.Clear();

		foreach (BPS_SafeZoneTriggerEntity zone : s_aMapZones)
		{
			if (zone)
				zones.Insert(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool BPS_ShouldShowMapBoundary()
	{
		if (!m_MapDisplayConfig || !m_MapDisplayConfig.IsEnabled())
			return false;

		if (!m_CampaignBase)
			return false;

		return m_CampaignBase.IsHQ();
	}

	//------------------------------------------------------------------------------------------------
	void BPS_BuildMapWorldOutline(notnull array<vector> points)
	{
		points.Clear();
		vector transform[4];
		GetWorldTransform(transform);

		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
		{
			points.Insert(Vector(-halfHorizontal, 0, -halfHorizontal).Multiply4(transform));
			points.Insert(Vector( halfHorizontal, 0, -halfHorizontal).Multiply4(transform));
			points.Insert(Vector( halfHorizontal, 0,  halfHorizontal).Multiply4(transform));
			points.Insert(Vector(-halfHorizontal, 0,  halfHorizontal).Multiply4(transform));
			return;
		}

		for (int i = 0; i < BPS_MAP_CYLINDER_SEGMENTS; i++)
		{
			float angle = (Math.PI2 * i) / BPS_MAP_CYLINDER_SEGMENTS;
			vector localPoint = Vector(
				Math.Cos(angle) * halfHorizontal,
				0,
				Math.Sin(angle) * halfHorizontal
			);

			points.Insert(localPoint.Multiply4(transform));
		}
	}

	//------------------------------------------------------------------------------------------------
	float BPS_GetMapBorderSize()
	{
		return m_MapDisplayConfig.GetBorderSize();
	}

	//------------------------------------------------------------------------------------------------
	vector BPS_GetMapWorldPosition()
	{
		return GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	float BPS_GetMapRadius()
	{
		return BPS_GetHorizontalSize() * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	bool BPS_IsEnemyMapZoneForLocalPlayer()
	{
		if (!m_CampaignBase)
			return false;

		Faction zoneFaction = m_CampaignBase.GetFaction();
		if (!zoneFaction)
			return false;

		Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (!localFaction)
		{
			int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
			if (localPlayerId > 0)
				localFaction = SCR_FactionManager.SGetPlayerFaction(localPlayerId);
		}

		if (!localFaction)
			return false;

		if (localFaction.GetFactionKey() == zoneFaction.GetFactionKey())
			return false;

		return !zoneFaction.IsFactionFriendly(localFaction);
	}

	//------------------------------------------------------------------------------------------------
	Color BPS_GetMapBorderColor()
	{
		if (BPS_IsEnemyMapZoneForLocalPlayer())
			return m_MapDisplayConfig.GetEnemyBorderColor();

		return m_MapDisplayConfig.GetFriendlyBorderColor();
	}

	//------------------------------------------------------------------------------------------------
	Color BPS_GetMapBackgroundColor()
	{
		if (BPS_IsEnemyMapZoneForLocalPlayer())
			return m_MapDisplayConfig.GetEnemyBackgroundColor();

		return m_MapDisplayConfig.GetFriendlyBackgroundColor();
	}

	// =============================================================================================
	// CONFIG HELPERS
	// =============================================================================================
	protected int GetIntruderDelay()
	{
		return m_IntruderConfig.GetKillDelaySeconds();
	}

	//------------------------------------------------------------------------------------------------
	protected float GetCombatDuration()
	{
		return m_CombatConfig.GetDuration();
	}

	// =============================================================================================
	// CHARACTER STATE
	// =============================================================================================
	protected BPS_CharacterState GetOrCreateCharacterState(IEntity character)
	{
		BPS_CharacterState existing = FindCharacterState(character);
		if (existing)
			return existing;

		BPS_CharacterState state = new BPS_CharacterState(character);
		m_aStates.Insert(state);
		return state;
	}

	//------------------------------------------------------------------------------------------------
	protected BPS_CharacterState FindCharacterState(IEntity character)
	{
		foreach (BPS_CharacterState state : m_aStates)
		{
			if (state && state.m_Character == character)
				return state;
		}

		return null;
	}

	// =============================================================================================
	// UI
	// =============================================================================================
	protected SCR_PlayerController GetController(IEntity character)
	{
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return null;

		int playerId = manager.GetPlayerIdFromControlledEntity(character);
		if (playerId <= 0)
			return null;

		return SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowMessage(IEntity character, string title, string message, float duration)
	{
		if (message == "")
			return;

		SCR_PlayerController controller = GetController(character);
		if (!controller)
			return;

		controller.BPS_ShowMessage(title, message, duration);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowMessageParam1(
		IEntity character,
		string title,
		string message,
		float duration,
		int param1
	)
	{
		SCR_PlayerController controller = GetController(character);
		if (!controller)
			return;

		controller.BPS_ShowMessageParam1(title, message, duration, param1);
	}

	// =============================================================================================
	// DEBUG VOLUME - NATIVE ENFUSION SHAPES
	// =============================================================================================
	protected ref Shape m_BPSRuntimeDebugShape;
#ifdef WORKBENCH
	protected ref Shape m_BPSWorkbenchDebugShape;
#endif

	//------------------------------------------------------------------------------------------------
	protected int BPS_GetDebugColor()
	{
		return Color.FromRGBA(0, 255, 70, 55).PackToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected ShapeFlags BPS_GetDebugShapeFlags()
	{
		return
			ShapeFlags.VISIBLE |
			ShapeFlags.NOZBUFFER |
			ShapeFlags.TRANSP |
			ShapeFlags.NOOUTLINE |
			ShapeFlags.ADDITIVE |
			ShapeFlags.DOUBLESIDE;
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_UpdateNativeDebugShapeTransform(Shape shape)
	{
		if (!shape)
			return;

		vector transform[4];
		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Cylindrical)
		{
			Math3D.MatrixIdentity4(transform);
			transform[3] = GetOrigin();
		}
		else
		{
			GetWorldTransform(transform);
		}

		shape.SetMatrix(transform);
	}

	//------------------------------------------------------------------------------------------------
	protected Shape BPS_CreateNativeCylinderDebugShape()
	{
		Shape shape = Shape.CreateCylinder(
			BPS_GetDebugColor(),
			BPS_GetDebugShapeFlags(),
			vector.Zero,
			BPS_GetHorizontalSize() * 0.5,
			BPS_GetHeight()
		);

		if (!shape)
			return null;

		BPS_UpdateNativeDebugShapeTransform(shape);
		return shape;
	}

	//------------------------------------------------------------------------------------------------
	protected Shape BPS_CreateNativeBoxDebugShape()
	{
		float halfHorizontal = BPS_GetHorizontalSize() * 0.5;
		float halfHeight = BPS_GetHeight() * 0.5;

		Shape shape = Shape.Create(
			ShapeType.BBOX,
			BPS_GetDebugColor(),
			BPS_GetDebugShapeFlags(),
			Vector(-halfHorizontal, -halfHeight, -halfHorizontal),
			Vector( halfHorizontal,  halfHeight,  halfHorizontal)
		);

		if (!shape)
			return null;

		BPS_UpdateNativeDebugShapeTransform(shape);
		return shape;
	}

	//------------------------------------------------------------------------------------------------
	protected Shape BPS_CreateNativeDebugShape()
	{
		if (!m_bShowDebugShape)
			return null;

		if (m_eTriggerShapeType == BPS_ETriggerShapeType.Square)
			return BPS_CreateNativeBoxDebugShape();

		return BPS_CreateNativeCylinderDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_CreateRuntimeDebugShape()
	{
		BPS_DestroyRuntimeDebugShape();

		if (!m_bShowDebugShape)
			return;

		if (System.IsConsoleApp())
			return;
#ifdef WORKBENCH
		if (!GetGame() || !GetGame().InPlayMode())
			return;
#endif

		m_BPSRuntimeDebugShape = BPS_CreateNativeDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_DestroyRuntimeDebugShape()
	{
		if (!m_BPSRuntimeDebugShape)
			return;

		delete m_BPSRuntimeDebugShape;
		m_BPSRuntimeDebugShape = null;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	protected void BPS_DestroyWorkbenchDebugShape()
	{
		if (!m_BPSWorkbenchDebugShape)
			return;

		delete m_BPSWorkbenchDebugShape;
		m_BPSWorkbenchDebugShape = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BPS_RebuildWorkbenchDebugShape()
	{
		BPS_DestroyWorkbenchDebugShape();
		if (!m_bShowDebugShape)
			return;

		if (GetGame() && GetGame().InPlayMode())
			return;

		m_BPSWorkbenchDebugShape = BPS_CreateNativeDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_OnInit(inout vector mat[4], IEntitySource src)
	{
		super._WB_OnInit(mat, src);
		if (src)
			BPS_WBSyncChangedProperty(src, "m_eTriggerShapeType");
		if (src)
			BPS_WBSyncChangedProperty(src, "m_fHorizontalSize");
		if (src)
			BPS_WBSyncChangedProperty(src, "m_fHeight");
		if (src)
			BPS_WBSyncChangedProperty(src, "m_bShowDebugShape");

		BPS_RebuildWorkbenchDebugShape();
	}

	//------------------------------------------------------------------------------------------------
	override bool _WB_OnKeyChanged(BaseContainer src, string key, BaseContainerList ownerContainers, IEntity parent)
	{
		if (BPS_WBSyncChangedProperty(src, key))
		{
			if (GetGame() && GetGame().InPlayMode())
				BPS_CreateRuntimeDebugShape();
			else
				BPS_RebuildWorkbenchDebugShape();
		}

		return super._WB_OnKeyChanged(src, key, ownerContainers, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected bool BPS_WBSyncChangedProperty(BaseContainer src, string key)
	{
		if (!src)
			return false;

		if (key == "m_eTriggerShapeType")
		{
			int shapeTypeValue;
			if (!src.Get(key, shapeTypeValue))
				return false;

			if (shapeTypeValue == BPS_ETriggerShapeType.Square)
				m_eTriggerShapeType = BPS_ETriggerShapeType.Square;
			else
				m_eTriggerShapeType = BPS_ETriggerShapeType.Cylindrical;

			return true;
		}

		if (key == "m_fHorizontalSize")
		{
			float horizontalSizeValue;
			if (!src.Get(key, horizontalSizeValue))
				return false;

			m_fHorizontalSize = horizontalSizeValue;
			return true;
		}

		if (key == "m_fHeight")
		{
			float heightValue;
			if (!src.Get(key, heightValue))
				return false;

			m_fHeight = heightValue;
			return true;
		}

		if (key == "m_bShowDebugShape")
		{
			bool showDebugValue;
			if (!src.Get(key, showDebugValue))
				return false;

			m_bShowDebugShape = showDebugValue;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		if (GetGame() && GetGame().InPlayMode())
			return;

		if (!m_bShowDebugShape)
		{
			BPS_DestroyWorkbenchDebugShape();
			return;
		}

		if (!m_BPSWorkbenchDebugShape)
		{
			BPS_RebuildWorkbenchDebugShape();
			return;
		}

		BPS_UpdateNativeDebugShapeTransform(m_BPSWorkbenchDebugShape);
	}
#endif

	// =============================================================================================
	// CLEANUP
	// =============================================================================================
	void ~BPS_SafeZoneTriggerEntity()
	{
		BPS_DestroyRuntimeDebugShape();
#ifdef WORKBENCH
		BPS_DestroyWorkbenchDebugShape();
#endif

		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(BPS_LogicTick);
				queue.Remove(BPS_TryBindCampaignBaseForMap);
				queue.Remove(BPS_TryInitialize);
			}
		}

		int mapIndex = s_aMapZones.Find(this);
		if (mapIndex >= 0)
			s_aMapZones.Remove(mapIndex);

		int index = s_aZones.Find(this);
		if (index >= 0)
			s_aZones.Remove(index);
	}
}
