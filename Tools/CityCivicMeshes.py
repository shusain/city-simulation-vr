"""Shared civic mesh recipes; scene/export orchestration stays in generators."""
import bpy
from CityArtKit import MATERIALS, box, combine, cylinder, wheel, material, initialize_materials


def initialize_civic_materials():
    initialize_materials()
    MATERIALS['Police'] = material('Police', (.12, .30, .65))
    MATERIALS['Fire'] = material('Fire', (.75, .10, .065))


def station(name, slot, fire=False):
    # Reserve the existing 24x16 m lot, while the starter building occupies
    # only 14x10 m. Future paid wings/garage bays can reuse this same footprint.
    parts = [box(name+'_Lot', (24,16,.14), (0,0,.07), 'Roof'),
             box(name+'_Body', (14,10,6), (-4,1,3), slot),
             box(name+'_Roof', (14.6,10.6,.5), (-4,1,6.25), 'Roof'),
             box(name+'_Driveway', (20,6.4,.16), (0,-7.2,.08), 'Roof'),
             box(name+'_Entry', (2.2,.3,3.4), (-8,-4.18,1.7), 'Roof'),
             box(name+'_Sign', (4,.5,.8), (-7,-4.35,4.9), slot),
             box(name+'_Emblem', (.8,.10,.8), (-7,-4.64,4.9), 'Roof')]
    for x in (0,):
        parts.append(box(name+'_Garage', (5,.18,4.3), (x,-4.1,2.15), 'Roof'))
        for z in (.8,1.7,2.6,3.5):
            parts.append(box(name+'_DoorRib', (4.8,.08,.10), (x,-4.23,z), slot))
    for x in (-9,-5,-1):
        parts.append(box(name+'_BackWindow', (2,.12,1.5), (x,6.08,3.6), 'Roof'))
    parts.append(box(name+'_FrontWindow', (2,.12,1.6), (-4,-4.08,3.5), 'Roof'))
    if fire:
        parts += [box(name+'_Tower', (3,3,9), (-8,4,4.5), slot),
                  box(name+'_TowerCap', (3.4,3.4,.4), (-8,4,9.2), 'Roof')]
    else:
        parts += [cylinder(name+'_Antenna', .12,2.5,(-7,4,7.75),'Roof'),
                  box(name+'_Radio', (2,2,.6),(-6,3,6.8),'Roof')]
    # A static decoration joined into the station mesh: no Actor or dispatch
    # state, and the same two semantic slots as the building. Nose faces -Y.
    parked = emergency_vehicle(name+'_ParkedVehicle', slot, fire)
    parked.rotation_euler.z = -1.57079632679
    parked.location = (7.3,1,.14)
    bpy.context.view_layer.objects.active = parked
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    parts.append(parked)
    for x in (5.4,9.2):
        parts.append(box(name+'_ParkingLine', (.10,8,.025),(x,1,.155),slot))
    parts.append(box(name+'_ParkingStop', (3.8,.10,.025),(7.3,5,.155),slot))
    return combine(name, parts)

def emergency_vehicle(name, slot, fire=False, *, ladder_rungs=True):
    length = 6.2 if fire else 3.8
    parts = [box(name+'_Body', (length,1.8,1.05 if fire else .75), (0,0,.78 if fire else .58), slot),
             box(name+'_Cab', (1.7,1.6,.8), (1.5 if fire else -.25,0,1.62 if fire else 1.25), 'Roof'),
             box(name+'_LightBar', (.35,1.45,.2), (1.5 if fire else -.25,0,2.12 if fire else 1.75), 'Roof' if not fire else slot)]
    colors_by_side = ((-.48,(1,.015,.01,0)),(.48,(1,.75,.55,0))) if fire else ((-.48,(1,.015,.01,0)),(.48,(.01,.12,1,0)))
    for side, color in colors_by_side:
        # Raise and enlarge the engine's lamps clear of the red light-bar housing.
        cap=box(name+'_RoofLight',(.75,.72,.56) if fire else (.41,.52,.24),(1.5 if fire else -.25,side,2.50 if fire else 1.75),'Roof')
        colors=cap.data.color_attributes.new(name='WindowMask', type='BYTE_COLOR', domain='CORNER')
        for value in colors.data: value.color=color
        parts.append(cap)
    for x in ((2,-2) if fire else (1.2,-1.2)):
        for y in (-.94,.94): parts.append(wheel(name+'_Wheel', .40 if fire else .33,.22,(x,y,.40 if fire else .33)))
    for y in (-.915,.915):
        parts.append(box(name+'_Stripe', (length-.3,.04,.22), (0,y,1.03 if fire else .78), 'Roof'))
    if fire:
        for y in (-.45,.45): parts.append(box(name+'_LadderRail', (4.3,.12,.14),(-.8,y,1.62),'Roof'))
        if ladder_rungs:
            for x in (-2.6,-2,-1.4,-.8,-.2,.4,1): parts.append(box(name+'_LadderRung', (.1,.9,.12),(x,0,1.62),'Roof'))
        for y in (-.94,.94): parts.append(box(name+'_Equipment', (2,.10,.7),(-1.3,y,.98),'Roof'))
    return combine(name, parts)
