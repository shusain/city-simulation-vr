"""D79 roof surfaces and atlas signs; only face materials/UVs change geometry."""
import copy,json
import bpy
from mathutils import Vector
from CityBuildingSurfaceGeometry import components

class RoofSigns:
    def __init__(self,root,material,char_trim):
        folder=root/'ArtSource/Textures/RoofsSigns'
        self.metadata=json.loads((folder/'roofs_signs.json').read_text(encoding='utf-8'))
        self.roofs={}
        for prefix in self.metadata['roof_tiles_m']:
            for damaged in (False,True):
                name=('Charred' if damaged else '')+prefix
                tint=(.12,.105,.09) if damaged else {'RoofShingles':(.24,.29,.33),'RoofMembrane':(.19,.23,.27),'RoofMetal':(.36,.40,.42)}[prefix]
                self.roofs[prefix,damaged]=material(name,tint,prefix,texture_dir=folder)
        self.signs={}
        for aspect in ('Wide','Tall'):
            m=material('StoreSign'+aspect,(1,1,1))
            p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Roughness'].default_value=.65
            n=m.node_tree.nodes.new('ShaderNodeTexImage')
            n.image=bpy.data.images.load(str(folder/('StoreSigns_'+aspect+'_BaseColor.png')),check_existing=True)
            n.extension='EXTEND';m.node_tree.links.new(n.outputs['Color'],p.inputs['Base Color'])
            self.signs[aspect]=m
        # Keep a distinct sign section on damaged variants with the same opaque
        # charred parent. Atlas selection must never restore a bright sign.
        self.char_sign=char_trim.copy();self.char_sign.name='CharredSign'

    def apply(self,obj,category,density,damaged,coords,normals):
        prefix='RoofShingles' if (category,density)==('Residential','Low') else 'RoofMetal' if category=='Industrial' else 'RoofMembrane'
        slot=len(obj.data.materials);obj.data.materials.append(self.roofs[prefix,damaged])
        # Intact coordinates are also used on sagged damaged roofs.
        if (category,density)==('Residential','Low'):
            parts=components(obj) if not damaged else []
            if not damaged:
                selected=[p for p in parts if abs(p['size'][0]-16)<.01 and abs(p['size'][1]-16)<.01 and abs(p['size'][2]-2)<.01]
                assert len(selected)==1, 'Missing residential gable roof'
                self.gable=set(selected[0]['vertices'])
            roof_faces=[p.index for p in obj.data.polygons if all(v in self.gable for v in p.vertices) and normals[p.index].z>.2]
        else:
            levels={('Residential','Medium'):(14,22),('Residential','High'):(36,),
                    ('Commercial','Low'):(8,),('Commercial','Medium'):(10,23),('Commercial','High'):(9,39),
                    ('Industrial','Low'):(8,),('Industrial','Medium'):(13,),('Industrial','High'):(18,)}[category,density]
            roof_faces=[p.index for p in obj.data.polygons if normals[p.index].z>.999 and
                        any(all(abs(coords[v].z-level)<.001 for v in p.vertices) for level in levels)]
        assert roof_faces, (category,density,'No roof surface')
        tile=self.metadata['roof_tiles_m'][prefix];uv=obj.data.uv_layers.active
        colors=obj.data.color_attributes.get('WindowMask')
        for index in roof_faces:
            p=obj.data.polygons[index];assert all(colors.data[i].color[3]>.999 for i in p.loop_indices)
            p.material_index=slot
            # Sloped gables use the sloping distance, preserving physical scale.
            tangent=normals[index].cross(Vector((1,0,0))).normalized()
            for i in p.loop_indices:
                c=coords[obj.data.loops[i].vertex_index]
                uv.data[i].uv=(c.x/tile,c.dot(tangent)/tile)
        detail=dict(roof_type=prefix,roof_tile_m=tile,roof_slot=slot,roof_faces=roof_faces,
                    mechanical_slot=2 if category=='Industrial' or (category,density)==('Residential','High') else None)
        if category=='Commercial':
            # Locate the accepted board by intact coordinates even on damage.
            dimensions={'Low':(5,1,4),'Medium':(7,1,3),'High':(8,1,4)}[density]
            parts=components(obj)
            matches=[]
            for part in parts:
                low=[min(coords[v][a] for v in part['vertices']) for a in range(3)]
                high=[max(coords[v][a] for v in part['vertices']) for a in range(3)]
                if all(abs(high[a]-low[a]-dimensions[a])<.001 for a in range(3)):matches.append((part,low,high))
            assert len(matches)==1, (category,density,'Missing sign board')
            part,low,high=matches[0];vertices=set(part['vertices'])
            sign_faces=[p.index for p in obj.data.polygons if all(v in vertices for v in p.vertices) and normals[p.index].y<-.999]
            assert sign_faces
            sign_slot=len(obj.data.materials)
            obj.data.materials.append(self.char_sign if damaged else self.signs['Tall' if density=='Low' else 'Wide'])
            for index in sign_faces:obj.data.polygons[index].material_index=sign_slot
            detail.update(sign_faces=sign_faces,sign_slot=sign_slot,sign_aspect='Tall' if density=='Low' else 'Wide',
                          sign_local_uv={str(i):[(coords[obj.data.loops[i].vertex_index].x-low[0])/dimensions[0],
                                               (coords[obj.data.loops[i].vertex_index].z-low[2])/dimensions[2]]
                                         for index in sign_faces for i in obj.data.polygons[index].loop_indices})
            detail=self.change_sign(obj,detail,{'Low':0,'Medium':2,'High':3}[density])
        return detail

    def change_sign(self,obj,detail,index):
        detail=copy.deepcopy(detail);detail['sign_atlas_index']=index
        height=1024 if detail['sign_aspect']=='Tall' else 512
        inset_x=4/2048;inset_y=4/height
        x=index%4/4;y=1-(index//4+1)/2
        for i,(u,v) in detail['sign_local_uv'].items():
            obj.data.uv_layers.active.data[int(i)].uv=(x+inset_x+u*(.25-2*inset_x),y+inset_y+v*(.5-2*inset_y))
        return detail
