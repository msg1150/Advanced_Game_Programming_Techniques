param([string]$OutputDirectory=(Join-Path $env:TEMP 'DXFramework-v017-tests'),[string]$Filter='*')
$ErrorActionPreference='Stop'
$sourceRoot=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot=(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if(!$vsRoot){throw 'MSVC x64 tools were not found.'}
$toolRoot=(Get-ChildItem (Join-Path $vsRoot 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$sdkVersion=(Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory | Where-Object {Test-Path (Join-Path $_.FullName 'um/Windows.h')} | Sort-Object Name -Descending | Select-Object -First 1).Name
$compiler=Join-Path $toolRoot 'bin/Hostx64/x64/cl.exe'
$env:PATH="$(Split-Path $compiler);$sdkRoot/bin/$sdkVersion/x64;"+$env:PATH
$env:INCLUDE="$toolRoot/include;$sdkRoot/Include/$sdkVersion/ucrt;$sdkRoot/Include/$sdkVersion/shared;$sdkRoot/Include/$sdkVersion/um;$sdkRoot/Include/$sdkVersion/winrt"
$env:LIB="$toolRoot/lib/x64;$sdkRoot/Lib/$sdkVersion/ucrt/x64;$sdkRoot/Lib/$sdkVersion/um/x64"
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$collision=@('Source/Physics/Collision/Core/CollisionWorld.cpp','Source/Physics/Collision/BroadPhase/BroadPhaseSystem.cpp','Source/Physics/Collision/Detection/CollisionDetector.cpp')
$xml=@('Source/UI/Document/Serialization/UIXmlParser.cpp','Source/UI/Document/Serialization/UIXmlWriter.cpp')
$ui=@('Source/UI/UIManager.cpp','Source/UI/UIWidget.cpp','Source/UI/Document/Runtime/UIWidgetFactory.cpp','Source/UI/Document/Runtime/UIDocumentLoader.cpp','Source/Features/UIDemo/UIDemoPanel.cpp')+$xml
$cases=[ordered]@{}
foreach($name in @('test_collision_shapes','test_collision_world')){$cases[$name]=$collision}
$cases['test_raycast']=@('Source/Physics/Query/Raycast.cpp')
foreach($name in @('test_ui_framework_layout','test_ui_placement','test_ui_scroll','test_ui_and_streaming_policy','test_prefetch_cache','test_streaming_timing','test_numeric_value','test_geomorph_policy','test_ui_binding_registry')){$cases[$name]=@()}
$cases['test_tile_archive']=@('Source/Features/DiskTerrainStreaming/TileArchive.cpp')
$cases['test_ui_document_xml']=$xml
$cases['test_ui_designer_model']=$xml+@('Source/Tools/UIDesigner/Core/UIDesignerModel.cpp')
foreach($name in @('test_ui_framework_interaction','test_ui_scroll_interaction','test_ui_document_factory')){$cases[$name]=$ui}
$cases['test_scene_document']=$collision+@('Source/Scene/Serialization/ColliderSceneDocument.cpp','Source/Scene/Runtime/ColliderSceneAdapter.cpp')
$cases['test_terrain_query']=@('Source/Terrain/Query/TerrainQuery.cpp','Source/Terrain/Query/DiskTerrainQuerySource.cpp','Source/Features/DiskTerrainStreaming/TileArchive.cpp')
$cases['test_engine_graphics']=@('Source/Engine/Core/Scene.cpp','Source/Engine/Serialization/SceneSerializer.cpp','Source/Engine/Graphics/GraphicsResources.cpp','Source/Graphics/Mesh.cpp','Source/Graphics/Shader.cpp','Source/Graphics/Texture2D.cpp')
$cases['test_engine_runtime']=$collision+@('Source/Engine/Core/Scene.cpp','Source/Engine/Serialization/SceneSerializer.cpp','Source/Engine/Collision/CollisionBody.cpp')
$cases['test_gpu_picking']=@('Source/Features/GPUPicking/GPUPickingSystem.cpp','Source/Features/CollisionEditor/CollisionGeometry.cpp','Source/Graphics/Shader.cpp')
function Invoke-CheckedProcess([string]$File,[string[]]$Arguments,[string]$LogPrefix,[int]$Timeout=120000)
{
    $process=Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $sourceRoot -WindowStyle Hidden -RedirectStandardOutput "$LogPrefix.stdout" -RedirectStandardError "$LogPrefix.stderr" -PassThru
    if(!$process.WaitForExit($Timeout)){$process.Kill();throw "Timed out: $File (logs: $LogPrefix)"}
    $process.Refresh()
    if($process.ExitCode -ne 0){Get-Content "$LogPrefix.stdout","$LogPrefix.stderr";throw "Exit $($process.ExitCode): $File"}
}
$results=@()
foreach($entry in ($cases.GetEnumerator() | Where-Object {$_.Key -like $Filter}))
{
    $name=$entry.Key;$folder=Join-Path $OutputDirectory $name
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    $exe=Join-Path $folder "$name.exe"
    $arguments=@('/nologo','/std:c++20','/EHsc','/utf-8','/W4','/UNDEBUG','/D_WIN32_WINNT=0x0601','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/I.','/ISource','/FITests/TestDiagnostics.h',"/Fo$($folder.Replace('\','/'))/",("/Fe"+'"'+$exe+'"'))
    if($name -in @('test_ui_framework_interaction','test_ui_scroll_interaction','test_ui_document_factory')){$arguments=@('/ITests/Mocks')+$arguments}
    $arguments+=@("Tests/$name.cpp")+$entry.Value+@('/link','/DEBUG:NONE','/INCREMENTAL:NO')
    if($name -in @('test_gpu_picking','test_engine_graphics')){$arguments+=@('d3d11.lib','dxgi.lib','d3dcompiler.lib','ole32.lib','windowscodecs.lib')}
    $rsp=Join-Path $folder 'compile.rsp'
    # Quote the directory-valued /Fo switch when TEMP/output paths contain spaces.
    $arguments=$arguments | ForEach-Object {if($_ -like '/Fo*'){ '/Fo"'+$_.Substring(3).TrimEnd('/')+'/"' }else{$_}}
    Set-Content -LiteralPath $rsp -Value $arguments -Encoding utf8
    Invoke-CheckedProcess $compiler @('@"'+$rsp+'"') (Join-Path $folder 'compile')
    $runArguments=@()
    if($name -in @('test_terrain_query','test_tile_archive')){$runArguments=@('"'+(Join-Path $sourceRoot 'Assets/TerrainTiles')+'"')}
    if($name -eq 'test_gpu_picking'){$runArguments=@('"'+(Join-Path $sourceRoot 'Shaders')+'"')}
    if($runArguments.Count -eq 0){$runArguments=@('--automated')}
    Invoke-CheckedProcess $exe $runArguments (Join-Path $folder 'run') 15000
    $results+= "$name PASS"
    Write-Output $results[-1]
}
$results | Set-Content (Join-Path $OutputDirectory 'results.txt') -Encoding utf8
Write-Output "$($results.Count) test executables passed. Runtime application was not launched."


