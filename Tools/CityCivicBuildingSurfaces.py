"""D91 selective asphalt roofs/tinted masonry; equipment keeps its finish."""
from pathlib import Path
import bpy
from mathutils import Vector
from CityBuildingSurfaceGeometry import components

def select_surfaces(obj,asset_id):
    parts=components(obj);mesh=obj.data
    if asset_id in ('building.park.city','SM_Building_CityPark_December_A'):
        roofs=[part for part in parts if all(abs(a-b)<.01 for a,b in zip(part['size'],(7.5,7.5,.4)))]
        if not roofs:return {},[],[]
        ids=set(i for part in roofs for i in part['vertices'])
        return {},[p.index for p in mesh.polygons if set(p.vertices).issubset(ids) and p.normal.z>.99],roofs
    # Grounded, room-sized closed boxes are building enclosures. Cylindrical
    # tanks, cooling towers, machinery on roofs, doors, signs and vehicles are
    # deliberately excluded. Small entrance/office rooms are included.
    enclosures=[part for part in parts if len(part['vertices'])==8 and part['low'][2]<1 and part['size'][2]>=2.9 and min(part['size'][:2])>=2 and part['size'][0]*part['size'][1]>=5]
    walls={};roofs=set();selected_parts=[]
    for enclosure in enclosures:
        ids=set(enclosure['vertices'])
        for p in mesh.polygons:
            if set(p.vertices).issubset(ids) and abs(p.normal.z)<.01:walls[p.index]=p.material_index
        cap=[]
        for part in parts:
            if len(part['vertices'])!=8 or not .08<=part['size'][2]<=1.1:continue
            if not all(-.01<=part['size'][i]-enclosure['size'][i]<=1.3 for i in (0,1)):continue
            if not all(abs((part['low'][i]+part['high'][i]-enclosure['low'][i]-enclosure['high'][i])/2)<.15 for i in (0,1)):continue
            if abs(part['low'][2]-enclosure['high'][2])>.35:continue
            cap.append(part)
        roof_parts=cap or [enclosure]
        for part in roof_parts:
            ids=set(part['vertices']);roofs.update(p.index for p in mesh.polygons if set(p.vertices).issubset(ids) and p.normal.z>.99)
        selected_parts.append(dict(low=enclosure['low'],high=enclosure['high'],size=enclosure['size'],roof_slab_count=len(cap)))
    assert set(walls).isdisjoint(roofs)
    return walls,sorted(roofs),selected_parts

def make_surface(root,name,tint,prefix,folder,normalization,strength):
    material=bpy.data.materials.new(name);material.diffuse_color=(*tint,1);material.use_nodes=True
    nodes=material.node_tree.nodes;links=material.node_tree.links;p=nodes.get('Principled BSDF')
    def image(suffix):
        n=nodes.new('ShaderNodeTexImage');n.image=bpy.data.images.load(str(root/folder/(prefix+'_'+suffix+'.png')),check_existing=True)
        if suffix!='BaseColor':n.image.colorspace_settings.name='Non-Color'
        n.extension='REPEAT';return n
    base=image('BaseColor');normal=image('Normal');orm=image('ORM')
    normalized=nodes.new('ShaderNodeMixRGB');normalized.blend_type='MULTIPLY';normalized.inputs[0].default_value=1;normalized.inputs[2].default_value=(*normalization,1);links.new(base.outputs['Color'],normalized.inputs[1])
    contrast=nodes.new('ShaderNodeMixRGB');contrast.inputs[0].default_value=strength;contrast.inputs[1].default_value=(1,1,1,1);links.new(normalized.outputs[0],contrast.inputs[2])
    color=nodes.new('ShaderNodeMixRGB');color.blend_type='MULTIPLY';color.inputs[0].default_value=1;color.inputs[2].default_value=(*tint,1);links.new(contrast.outputs[0],color.inputs[1]);links.new(color.outputs[0],p.inputs['Base Color'])
    split=nodes.new('ShaderNodeSeparateColor');links.new(orm.outputs['Color'],split.inputs[0]);links.new(split.outputs['Green'],p.inputs['Roughness'])
    separate=nodes.new('ShaderNodeSeparateColor');links.new(normal.outputs['Color'],separate.inputs[0])
    inverse=nodes.new('ShaderNodeMath');inverse.operation='SUBTRACT';inverse.inputs[0].default_value=1;links.new(separate.outputs['Green'],inverse.inputs[1])
    combine=nodes.new('ShaderNodeCombineColor');links.new(separate.outputs['Red'],combine.inputs['Red']);links.new(inverse.outputs[0],combine.inputs['Green']);links.new(separate.outputs['Blue'],combine.inputs['Blue'])
    normal_node=nodes.new('ShaderNodeNormalMap');links.new(combine.outputs[0],normal_node.inputs['Color']);links.new(normal_node.outputs[0],p.inputs['Normal'])
    return material

def apply_surfaces(obj,walls,roofs,wall_materials,roof_material):
    mesh=obj.data;uv=mesh.uv_layers.active or mesh.uv_layers.new(name='UVMap')
    wall_slots={}
    for old,material in wall_materials.items():wall_slots[old]=len(mesh.materials);mesh.materials.append(material)
    roof_slot=len(mesh.materials);mesh.materials.append(roof_material)
    for index,old in walls.items():mesh.polygons[index].material_index=wall_slots[old]
    for index in roofs:mesh.polygons[index].material_index=roof_slot
    for index in set(walls)|set(roofs):
        face=mesh.polygons[index];axis=max(range(3),key=lambda i:abs(face.normal[i]));sign=1 if face.normal[axis]>0 else -1
        for li in face.loop_indices:
            c=mesh.vertices[mesh.loops[li].vertex_index].co
            if index in walls:
                a,b=(sign*c.y,c.z) if axis==0 else (-sign*c.x,c.z);uv.data[li].uv=(a/2.4,b/2.4)
            else:uv.data[li].uv=(c.x/2,c.y/2)
    return wall_slots,roof_slot
