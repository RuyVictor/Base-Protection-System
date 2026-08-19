//------------------------------------------------------------------------------------------------
// BPS - Base Protection System
// Map Safe Zone boundary rendering.
//
// Uses a client-side CanvasWidget so the boundary can have:
// - solid background/fill color
// - configurable border color
// - configurable border thickness
//
// Radius always comes from BPS_SafeZoneTriggerEntity.GetSphereRadius().
//------------------------------------------------------------------------------------------------

modded class SCR_MapEntity
{
	protected static const int BPS_MAP_CIRCLE_SEGMENTS = 96;

	protected CanvasWidget m_BPSOverlayCanvas;

	protected ref array<ref CanvasWidgetCommand> m_aBPSDrawCommands =
		new array<ref CanvasWidgetCommand>();

	protected ref array<BPS_SafeZoneTriggerEntity> m_aBPSMapZones =
		new array<BPS_SafeZoneTriggerEntity>();


	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);


		if (!IsOpen())
		{
			BPS_DestroyOverlay();
			return;
		}


		if (!BPS_EnsureOverlay())
			return;


		BPS_UpdateOverlay();
	}


	//------------------------------------------------------------------------------------------------
	protected bool BPS_EnsureOverlay()
	{
		if (m_BPSOverlayCanvas)
			return true;


		WorkspaceWidget workspace =
			GetGame().GetWorkspace();


		if (!workspace)
			return false;


		// Attach the BPS canvas to the same UI level as the native map widget,
		// not directly at the top of MapMenuRoot. This keeps spawn selection
		// widgets, labels and other map UI layers above the Safe Zone overlay.
		CanvasWidget mapWidget =
			GetMapWidget();


		Widget overlayParent =
			GetMapMenuRoot();


		int overlayZOrder = -1000;


		if (mapWidget)
		{
			Widget mapParent =
				mapWidget.GetParent();


			if (mapParent)
				overlayParent = mapParent;


			// One layer above the map itself, while remaining below the
			// higher UI layers used by spawn buttons and map text.
			overlayZOrder =
				mapWidget.GetZOrder() + 1;
		}


		if (!overlayParent)
			return false;


		Widget overlay =
			workspace.CreateWidget(
				WidgetType.CanvasWidgetTypeID,
				WidgetFlags.VISIBLE |
					WidgetFlags.IGNORE_CURSOR |
					WidgetFlags.NOFOCUS,
				null,
				overlayZOrder,
				overlayParent
			);


		m_BPSOverlayCanvas =
			CanvasWidget.Cast(overlay);


		if (!m_BPSOverlayCanvas)
			return false;


		m_BPSOverlayCanvas.SetName(
			"BPS_SafeZoneMapOverlay"
		);


		m_BPSOverlayCanvas.SetZOrder(
			overlayZOrder
		);


		// Fill the same frame used by the native map widget.
		FrameSlot.SetAnchorMin(
			m_BPSOverlayCanvas,
			0,
			0
		);

		FrameSlot.SetAnchorMax(
			m_BPSOverlayCanvas,
			1,
			1
		);

		FrameSlot.SetOffsets(
			m_BPSOverlayCanvas,
			0,
			0,
			0,
			0
		);


		m_BPSOverlayCanvas.SetFlags(
			WidgetFlags.IGNORE_CURSOR |
			WidgetFlags.NOFOCUS
		);


		return true;
	}


	//------------------------------------------------------------------------------------------------
	protected void BPS_UpdateOverlay()
	{
		if (!m_BPSOverlayCanvas)
			return;


		m_aBPSDrawCommands.Clear();


		BPS_SafeZoneTriggerEntity.BPS_CopyMapZones(
			m_aBPSMapZones
		);


		float pixelsPerMeter =
			GetCurrentZoom();


		foreach (
			BPS_SafeZoneTriggerEntity zone :
			m_aBPSMapZones
		)
		{
			if (!zone)
				continue;


			if (!zone.BPS_ShouldShowMapBoundary())
				continue;


			vector worldPos =
				zone.BPS_GetMapWorldPosition();


			int screenX;
			int screenY;


			WorldToScreen(
				worldPos[0],
				worldPos[2],
				screenX,
				screenY,
				true
			);


			float radiusPx =
				zone.BPS_GetMapRadius() *
				pixelsPerMeter;


			if (radiusPx <= 0)
				continue;


			ref array<float> vertices =
				new array<float>();


			m_BPSOverlayCanvas.TessellateCircle(
				Vector(screenX, screenY, 0),
				radiusPx,
				BPS_MAP_CIRCLE_SEGMENTS,
				vertices
			);


			// -----------------------------------------------------------------
			// BACKGROUND / FILL
			// -----------------------------------------------------------------

			ref PolygonDrawCommand fillCommand =
				new PolygonDrawCommand();


			fillCommand.m_Vertices =
				vertices;

			fillCommand.m_iColor =
				zone
					.BPS_GetMapBackgroundColor()
					.PackToInt();


			m_aBPSDrawCommands.Insert(
				fillCommand
			);


			// -----------------------------------------------------------------
			// BORDER
			// -----------------------------------------------------------------

			ref LineDrawCommand borderCommand =
				new LineDrawCommand();


			borderCommand.m_Vertices =
				vertices;

			borderCommand.m_iColor =
				zone
					.BPS_GetMapBorderColor()
					.PackToInt();

			borderCommand.m_fWidth =
				zone.BPS_GetMapBorderSize();

			borderCommand.m_bShouldEnclose =
				true;


			m_aBPSDrawCommands.Insert(
				borderCommand
			);
		}


		m_BPSOverlayCanvas.SetDrawCommands(
			m_aBPSDrawCommands
		);
	}


	//------------------------------------------------------------------------------------------------
	protected void BPS_DestroyOverlay()
	{
		if (!m_BPSOverlayCanvas)
			return;


		m_aBPSDrawCommands.Clear();

		m_BPSOverlayCanvas.SetDrawCommands(
			m_aBPSDrawCommands
		);

		m_BPSOverlayCanvas.RemoveFromHierarchy();
		m_BPSOverlayCanvas = null;
	}
}
