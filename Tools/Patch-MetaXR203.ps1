# Equivalent infinity expression avoids MSVC C4756 in Meta XR 203.0.
$ErrorActionPreference = 'Stop'
$taskSource = Join-Path (Split-Path $PSScriptRoot -Parent) 'CitySimVR\Plugins\MetaXR\Source\OculusXRHMD\Private\OculusXRHMD_Layer.cpp'
$taskText = [IO.File]::ReadAllText($taskSource)
$taskOld = 'OvrpLayerSubmit.EyeFov.MotionVectorDepthNear = INFINITY;'
$taskNew = 'OvrpLayerSubmit.EyeFov.MotionVectorDepthNear = std::numeric_limits<float>::infinity();'
if ($taskText.Contains($taskNew)) { return }
if (-not $taskText.Contains($taskOld)) { throw 'Expected Meta XR 203.0 source expression was not found.' }
$taskText = $taskText.Replace('#include "OculusXRHMD_Layer.h"', "#include `"OculusXRHMD_Layer.h`"`n#include <limits>")
$taskText = $taskText.Replace($taskOld, $taskNew)
[IO.File]::WriteAllText($taskSource, $taskText)
