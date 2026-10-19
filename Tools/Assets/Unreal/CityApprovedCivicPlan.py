"""Pinned #157 batch inputs and exact source graph extraction; no Unreal dependency."""
from pathlib import Path
import hashlib,json,sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export
from Assets.Unreal.CityApprovedUtilityTemplates import Source as UtilitySource
SCOPE='/Game/CitySim/Art/ApprovedCivicBatch'
PLAN=ROOT/'ArtSource/Manifests/city_civic_batch.json'
CONCRETE='/Game/CitySim/Art/HomeGroundPilot/M_Concrete'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_bytes())
def write(p,j):Path(p).write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8',newline='\n')

class Source(UtilitySource):
    def uvscale(self,image):
        ref=self.linked(image,self.index(image,'Vector'))
        if ref is not None and self.nodes[ref[0]]['type']=='ShaderNodeTexCoord':
            self.typed(ref,'ShaderNodeTexCoord','UV');return 1.,dict(node=ref[0],input='UV')
        return super().uvscale(image)
    def normal(self):
        node=self.typed(self.ref(self.bsdf,self.index(self.bsdf,'Normal')),'ShaderNodeNormalMap','Normal')
        self.image('Normal',self.ref(node,self.index(node,'Color')),'normal')
        self.scalar('NormalStrength',node,self.index(node,'Strength'))
    def solid(self):
        ref=self.linked(self.bsdf,self.index(self.bsdf,'Normal'))
        if ref is None:return super().solid()
        self.vector('BaseColor',self.bsdf,self.index(self.bsdf,'Base Color'))
        self.scalar('Roughness',self.bsdf,self.index(self.bsdf,'Roughness'))
        self.scalar('Metallic',self.bsdf,self.index(self.bsdf,'Metallic'))
        self.normal();return 'SolidNormal'
    def pbr(self,base_ref):
        rough=self.typed(self.ref(self.bsdf,self.index(self.bsdf,'Roughness')),'ShaderNodeSeparateColor','Green')
        metal=self.typed(self.ref(self.bsdf,self.index(self.bsdf,'Metallic')),'ShaderNodeSeparateColor','Blue')
        assert rough['name']==metal['name']
        ao=0.;tint=[1.,1.,1.,1.];has_tint=False;origins=[]
        while self.nodes[base_ref[0]]['type']=='ShaderNodeMixRGB':
            mix=self.typed(base_ref,'ShaderNodeMixRGB','Color')
            assert mix['blend_type']=='MULTIPLY' and not mix.get('use_clamp')
            factor=self.constant(mix,0);assert 0<=factor<=1
            other=self.linked(mix,2);origins.append(mix['name'])
            if other is None:
                c=self.constant(mix,2);assert isinstance(c,list)
                tint=[a*((1-factor)+factor*b) for a,b in zip(tint,c)];has_tint=True
            else:
                red=self.typed(other,'ShaderNodeSeparateColor','Red');assert red['name']==rough['name'] and ao==0
                ao=float(factor)
            base_ref=self.ref(mix,1)
        self.image('BaseColor',base_ref,'color');self.image('ORM',self.ref(rough,0),'orm');self.normal()
        self.add('SourceTintColor','vector',tint,origins);self.add('SourceTintWeight','scalar',1. if has_tint else 0.,origins)
        self.add('SourceAOWeight','scalar',ao,origins);self.add('RoughnessScale','scalar',1.,'source ORM.G');self.add('MetallicScale','scalar',1.,'source ORM.B')
        return 'PBR'
    def compile(self):
        ref=self.linked(self.bsdf,self.index(self.bsdf,'Base Color'));template=self.solid() if ref is None else self.pbr(ref)
        self.add('SurfaceTint','vector',[1,1,1]);self.add('DamageSolidColor','vector',[.065,.055,.045]);self.add('DamageSolidWeight','scalar',0.);self.add('DamageRoughnessWeight','scalar',0.)
        parent=(SCOPE+'/Materials/M_Template_SolidNormal') if template=='SolidNormal' else '/Game/CitySim/Art/Utilities/ApprovedBatch/Materials/M_Template_'+template
        return dict(template=template,parent=parent,parameters=self.params,source_graph_sha256=sha_bytes(self.graph),omitted_source_detail=self.omitted)

def sha_bytes(j):return hashlib.sha256(json.dumps(j,sort_keys=True).encode()).hexdigest()
def textures(plan):
    result={}
    for a in plan['assets']:
        for t in a['textures']:
            kind='color' if t['colorspace']=='sRGB' else 'normal' if 'Normal' in t['exported_path'] else 'orm'
            result[t['sha256']]=dict(sha256=t['sha256'],source=t['reuse_source_paths'][0],native=t['candidate_native_texture'],kind=kind)
    return list(result.values())
def preflight():
    plan=read(PLAN);assert plan['issue']==157 and len(plan['assets'])==15 and plan['scope']==SCOPE
    for f,pin in plan['protected_sources'].items():assert sha(ROOT/f)==pin,('Approved original changed',f)
    ts=textures(plan)
    for t in ts:assert sha(ROOT/t['source'])==t['sha256']
    for a in plan['assets']:
        verified_export(ROOT/a['export_directory'],a['issue'],a['version'],a['approval_sha256'],kind='blender')
        for lod in a['lods']:assert sha(ROOT/lod['export'])==lod['export_sha256']
        a['compiled_materials']={g['region']:Source(g,ts).compile() for g in a['graphs']}
    plan['textures']=ts;plan['stock_concrete']=CONCRETE
    return plan

if __name__=='__main__':
    p=preflight();print('CIVIC_BATCH_PREFLIGHT_PASS',len(p['assets']),len(p['textures']),sum(len(a['graphs']) for a in p['assets']))
