//------------------------------------------------------------------------------------------------
// BPS - CONFIGURATION
//------------------------------------------------------------------------------------------------


// ================================================================================================
// FRIENDLY MESSAGES
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_FriendlyMessagesConfig
{
	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Show messages when friendly players enter or leave."
	)]
	protected bool m_bEnabled = true;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Message duration."
	)]
	protected float m_fDuration = 4.0;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sEnterTitle = "SAFE ZONE";


	[Attribute(
		"You entered a safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sEnterMessage = "You entered a safe zone.";


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sExitTitle = "SAFE ZONE";


	[Attribute(
		"You left the safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sExitMessage = "You left the safe zone.";


	bool IsEnabled()
	{
		return m_bEnabled;
	}


	float GetDuration()
	{
		return m_fDuration;
	}


	string GetEnterTitle()
	{
		return m_sEnterTitle;
	}


	string GetEnterMessage()
	{
		return m_sEnterMessage;
	}


	string GetExitTitle()
	{
		return m_sExitTitle;
	}


	string GetExitMessage()
	{
		return m_sExitMessage;
	}
}


// ================================================================================================
// ENEMY INTRUSION
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_IntruderConfig
{
	[Attribute(
		"10",
		UIWidgets.EditBox,
		"Seconds an enemy can remain inside before being killed."
	)]
	protected int m_iKillDelaySeconds = 10;


	[Attribute(
		"1.1",
		UIWidgets.EditBox,
		"Countdown message duration."
	)]
	protected float m_fCountdownMessageDuration = 1.1;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Exit message duration."
	)]
	protected float m_fExitMessageDuration = 4.0;


	[Attribute(
		"WARNING - ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sWarningTitle = "WARNING - ENEMY SAFE ZONE";


	[Attribute(
		"You are inside an enemy Safe Zone. Leave immediately. You will be killed in %1 seconds.",
		UIWidgets.EditBox,
		"Supports localization keys. %1 = remaining seconds."
	)]
	protected string m_sWarningMessage =
		"You are inside an enemy Safe Zone. Leave immediately. You will be killed in %1 seconds.";


	[Attribute(
		"ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sExitTitle = "ENEMY SAFE ZONE";


	[Attribute(
		"You left the enemy Safe Zone. Elimination cancelled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sExitMessage =
		"You left the enemy Safe Zone. Elimination cancelled.";


	int GetKillDelaySeconds()
	{
		if (m_iKillDelaySeconds < 1)
			return 1;

		return m_iKillDelaySeconds;
	}


	float GetCountdownMessageDuration()
	{
		return m_fCountdownMessageDuration;
	}


	float GetExitMessageDuration()
	{
		return m_fExitMessageDuration;
	}


	string GetWarningTitle()
	{
		return m_sWarningTitle;
	}


	string GetWarningMessage()
	{
		return m_sWarningMessage;
	}


	string GetExitTitle()
	{
		return m_sExitTitle;
	}


	string GetExitMessage()
	{
		return m_sExitMessage;
	}
}


// ================================================================================================
// COMBAT LOCK
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_CombatConfig
{
	[Attribute(
		"15",
		UIWidgets.EditBox,
		"Combat Lock duration. 0 disables Combat Lock."
	)]
	protected float m_fDuration = 15.0;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Combat messages duration."
	)]
	protected float m_fMessageDuration = 4.0;


	[Attribute(
		"SAFE ZONE - COMBAT",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sLockTitle = "SAFE ZONE - COMBAT";


	[Attribute(
		"You fired inside the Safe Zone. Your protection has been temporarily disabled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sLockMessage =
		"You fired inside the Safe Zone. Your protection has been temporarily disabled.";


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sRestoredTitle = "SAFE ZONE";


	[Attribute(
		"Your Safe Zone protection has been restored.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sRestoredMessage =
		"Your Safe Zone protection has been restored.";


	float GetDuration()
	{
		if (m_fDuration < 0)
			return 0;

		return m_fDuration;
	}


	float GetMessageDuration()
	{
		return m_fMessageDuration;
	}


	string GetLockTitle()
	{
		return m_sLockTitle;
	}


	string GetLockMessage()
	{
		return m_sLockMessage;
	}


	string GetRestoredTitle()
	{
		return m_sRestoredTitle;
	}


	string GetRestoredMessage()
	{
		return m_sRestoredMessage;
	}
}


// ================================================================================================
// FRIENDLY FIRE
// ================================================================================================

[BaseContainerProps(configRoot: true)]
class BPS_FriendlyFireConfig
{
	[Attribute(
		"1",
		UIWidgets.CheckBox,
		"Block friendly damage against a friendly player who is inside the Safe Zone."
	)]
	protected bool m_bEnabled = true;


	[Attribute(
		"2",
		UIWidgets.EditBox,
		"Warning cooldown."
	)]
	protected float m_fWarningCooldown = 2.0;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Warning duration."
	)]
	protected float m_fMessageDuration = 4.0;


	[Attribute(
		"FRIENDLY FIRE BLOCKED",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sTitle = "FRIENDLY FIRE BLOCKED";


	[Attribute(
		"You cannot damage allies inside the Safe Zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	protected string m_sMessage =
		"You cannot damage allies inside the Safe Zone.";


	bool IsEnabled()
	{
		return m_bEnabled;
	}


	float GetWarningCooldown()
	{
		return m_fWarningCooldown;
	}


	float GetMessageDuration()
	{
		return m_fMessageDuration;
	}


	string GetTitle()
	{
		return m_sTitle;
	}


	string GetMessage()
	{
		return m_sMessage;
	}
}