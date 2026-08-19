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
	bool m_bEnabled;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Message duration."
	)]
	float m_fDuration;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sEnterTitle;


	[Attribute(
		"You entered a safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sEnterMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitTitle;


	[Attribute(
		"You left the safe zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitMessage;


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
	int m_iKillDelaySeconds;


	[Attribute(
		"1.1",
		UIWidgets.EditBox,
		"Countdown message duration."
	)]
	float m_fCountdownMessageDuration;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Exit message duration."
	)]
	float m_fExitMessageDuration;


	[Attribute(
		"WARNING - ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sWarningTitle;


	[Attribute(
		"You are inside an enemy Safe Zone. Leave immediately. You will be killed in %1 seconds.",
		UIWidgets.EditBox,
		"Supports localization keys. %1 = remaining seconds."
	)]
	string m_sWarningMessage;


	[Attribute(
		"ENEMY SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitTitle;


	[Attribute(
		"You left the enemy Safe Zone. Elimination cancelled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sExitMessage;


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
	float m_fDuration;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Combat messages duration."
	)]
	float m_fMessageDuration;


	[Attribute(
		"SAFE ZONE - COMBAT",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sLockTitle;


	[Attribute(
		"You fired inside the Safe Zone. Your protection has been temporarily disabled.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sLockMessage;


	[Attribute(
		"SAFE ZONE",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sRestoredTitle;


	[Attribute(
		"Your Safe Zone protection has been restored.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sRestoredMessage;


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
	bool m_bEnabled;


	[Attribute(
		"2",
		UIWidgets.EditBox,
		"Warning cooldown."
	)]
	float m_fWarningCooldown;


	[Attribute(
		"4",
		UIWidgets.EditBox,
		"Warning duration."
	)]
	float m_fMessageDuration;


	[Attribute(
		"FRIENDLY FIRE BLOCKED",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sTitle;


	[Attribute(
		"You cannot damage allies inside the Safe Zone.",
		UIWidgets.EditBox,
		"Supports localization keys."
	)]
	string m_sMessage;


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