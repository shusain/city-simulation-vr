"""Consolidate completed D83 local gates without regenerating assets or baselines."""
from pathlib import Path
import json,hashlib,re,datetime
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D83/Integration'
read=lambda p:json.loads((ROOT/p).read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def relative(p):return p.relative_to(ROOT).as_posix()
def report_from_log(name):
    text=(OUT/name).read_text(encoding='utf-8-sig')
    match=re.search(r'Report: (.+Report.json)',text);assert match,name
    p=Path(match.group(1).strip());return p, json.loads(p.read_text()),json.loads(p.with_name('Metadata.json').read_text(encoding='utf-8-sig'))
control_path,control,control_meta=report_from_log('Timing-Static.log')
active_path,active,active_meta=report_from_log('Timing-Animated.log')
for key in ('executableSha256','sourceSha256','width','height','warmupSeconds','durationSeconds','hud','lighting','sceneScale','msaaSamples','weather','speed'):
    if key in control_meta:assert control_meta[key]==active_meta[key],key
for key in ('width','height','authoritativeStateSha1','captureStartStep','captureEndStep','captureStartMonth','captureEndMonth','buildingTypes','terrainSeed'):
    assert control[key]==active[key],key
assert control_meta['screenshotDuringCapture']==active_meta['screenshotDuringCapture']==False
assert '-CityStaticParkCitizens' in control_meta['arguments'] and '-CityStaticParkCitizens' not in active_meta['arguments']
assert '-CityBenchmarkParkMotion' in control_meta['arguments'] and '-CityBenchmarkParkMotion' in active_meta['arguments']
assert control['simulationPaused'] and active['simulationPaused'] and control['captureStartStep']==control['captureEndStep']
for name,count in (('Full.log',253),('Parks.log',6)):
    text=(OUT/name).read_text(errors='replace')
    assert len(re.findall(r'Test Completed\. Result=\{Success\}',text))==count,name
    assert not re.search(r'Result=\{Fail|Ensure condition failed|Handled ensure|Fatal error',text),name
assert 'passed for 288 project files' in (OUT/'Formatting.log').read_text()
for name in ('WindowsPackage.log','QuestPackage.log'):assert 'BUILD SUCCESSFUL' in (OUT/name).read_text(errors='replace')
assert '26 captures' in (OUT/'DesktopSmoke.log').read_text()
assert 'min32/target35, signed, embedded cooked payload' in (OUT/'QuestPayload.log').read_text()
doc=read('ArtSource/Manifests/city_park_integration.json');assert doc['native_selected']
native=read('Artifacts/D83/Integration/NativePersistence.json');assert len(native['assets'])==43 and native['read_only']
preview=read('Artifacts/D83/Integration/Preview/Report.json');assert not preview['error'] and len(preview['views'])==13
assert preview['views'][-1]['animated']==0 and preview['views'][0]['animated']==27
static=read('Artifacts/D83/Integration/StaticPreview/Report.json');assert not static['error'] and len(static['views'])==36
protection=read('Artifacts/D83/Integration/Preservation.json');assert protection['protected']==1250 and protection['other_inputs_unchanged']
metrics=('frameMeanMs','frameP95Ms','frameP99Ms','framePeakMs','gameMeanMs','renderMeanMs','gpuMeanMs','rhiDrawCallsMean','framesOver30BudgetPercent')
cost={k:dict(static=control[k],animated=active[k],delta=active[k]-control[k]) for k in metrics}
package_files=[ROOT/'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe']
package_files+=sorted((ROOT/'Artifacts/PCVRSandbox/Windows/CitySimVR/Content/Paks').glob('*'))
package_files+=[ROOT/'Artifacts/QuestSandbox/Android_ASTC/CitySimVR-arm64.apk']
packages={relative(p):dict(bytes=p.stat().st_size,sha256=sha(p)) for p in package_files}
assert packages[relative(package_files[0])]['sha256'].upper()==active_meta['executableSha256']
result=dict(task='D83',local_integration_complete=True,updated_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    exports=dict(static_pairs=18,skeletal_exports=12,native_skeletal_meshes=6,bones=20,triangles=[754,420],clips=14,props=5,
        static_fallbacks=6,maximum_deformed_roundtrip_error_m=max(e['roundtrip_pose_error_m'] for e in doc['clips']),
        accepted_original_sources_preserved=True,source_inputs=doc['source_inputs']),
    runtime=dict(owner='existing CityBoardActor presentation clock',desktop_cap=32,desktop_pose_hz=30,quest_cap=8,quest_pose_hz=15,
        android_mesh_lod=1,per_visitor_ticks=False,route_walkers_unchanged=True,authoritative_simulation_unchanged=True,
        motions='12 accepted park activities plus idle/walk; source world carrier/board/balls synchronized; one-shot hidden recycle',
        presentation_only=True),
    tests=dict(focused=6,full=253,formatting_files=288,editor_warnings_as_errors=True,static_lod_views=36,park_views=13,
        packaged_desktop_resolution=[3840,2160],packaged_desktop_captures=26,quest_signed_embedded_arm64_min32_target35=True),
    protection=protection,packages=packages,
    animation_cost=dict(control=relative(control_path),animated=relative(active_path),metrics=cost,
        source_sha256=active_meta['sourceSha256'],executable_sha256=active_meta['executableSha256'],
        authoritative_sha1=active['authoritativeStateSha1'],warmup_seconds=10,sample_seconds=45,
        hardware=dict(cpu=active['cpu'],gpu=active['gpu'],rhi=active['rhi']),
        scope='Same executable, seven-park copied city, paused authority and active cosmetics, clear/day,4K D3D11,4xMSAA,45s each; screenshots captured in separate runs',
        limitations='Sequential desktop measurements include whole renderer/UI and hardware scheduling; no isolated per-bone CPU attribution or dense-city/device/PCVR timing proof'),
    evidence=['Native-Parks.png','Native-Skate.gif','Native-Variants.png','NativePersistence.json','Promotion.json','Preservation.json',
        'AcceptedSourceVerification.log','IndexPreservation.json','IndexBackupRecovery.json'],
    pending=['Physical Desktop input/appearance acceptance','PCVR stereo/controller/comfort and refresh timing',
        'Quest appearance/lifecycle/thermal frame timing and8-component cap on physical device',
        'Broader gait/push propulsion polish or event-driven sports only if separately requested'],
    installed=False,published=False,d58_imported=False,next='D72 coordinator owns subsequent D58 MetaSound integration and combined test bundle after explicit D83 window release')
if (OUT/'Verification.json').exists():
    previous=json.loads((OUT/'Verification.json').read_text())
    for key in ('native_window_released','heartbeat_status'):
        if key in previous:result[key]=previous[key]
(OUT/'Verification.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D83_LOCAL_INTEGRATION_VERIFIED')
