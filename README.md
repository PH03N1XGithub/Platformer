# Rebel Engine Game Guide

## Build Instructions

For full build setup and instructions, see the Rebel Engine repository:
https://github.com/PH03N1XGithub/Rebel-Engine

## Overview

This is the gameplay guide for the platformer mini-game built in Rebel Engine.

## Levels

- `Editor/MainLevel.Ryml`: Main hub with 3 portals.
- `Editor/PlatfromerGame.Ryml`: Parkour Level 1.
- `Editor/ParkourLevel2.Ryml`: Parkour Level 2.
- `Editor/ParkourLevel3.Ryml`: Parkour Level 3.

## Level Flow

From `MainLevel.Ryml`:

- `Portal_Parkour_1` -> `PlatfromerGame.Ryml`
- `Portal_Parkour_2` -> `ParkourLevel2.Ryml`
- `Portal_Parkour_3` -> `ParkourLevel3.Ryml`

Each parkour level has a portal at the end that returns to `MainLevel.Ryml`.

## Controls

- `W / A / S / D`: Move
- `Space`: Jump
- `Left Shift`: Dash

## Movement Mechanics

- Coyote time is enabled in character movement.
- Double-jump behavior is available via the movement setup.
- Wall run can start when pressing `Space` while airborne near a runnable wall.
- Wall jump triggers when pressing `Space` during wall run.

## Timer

- Timer is shown in the viewport while Play-In-Editor is active.
- Timer visibility is controlled by `LevelStarterActor.m_bEnableLevelTimer`.
- Timer resets when the level starts and tracks elapsed level time.

## Level Reset (Fall Check)

`LevelStarterActor` handles fall reset using a Z threshold:

- If player Z is below `m_FallResetZ` (default `-20`), player teleports to `m_ResetLocation` (default `0,0,0`).
- If `m_bResetVelocityOnTeleport` is enabled, velocity is cleared after teleport.

## Portal Overlap Rules

`LevelPortalActor` uses overlap triggers:

- Uses a trigger `BoxComponent`.
- Pawn overlap can be required (`m_bRequirePawnOverlap`).
- Scene transition delay is controlled by `m_TransitionDelaySeconds`.
- Destination scene is `m_TargetScenePath`.
