"""Synthetic geometry and scene compatibility tests; no original game art."""
import unittest
import sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/living'))
from city_airspace import terrain_triangles,possible_collision
from expanded_scenes import extend,city_faction
from prepare_static import opaque_material
from human_routes import build_graph
import struct

class Expanded(unittest.TestCase):
    def test_pedestrian_branches_follow_real_platform_and_reject_wall(self):
        floor=np.array([[[-20,10,-20],[20,10,-20],[20,10,20]],[[-20,10,-20],[20,10,20],[-20,10,20]]],np.float32)
        wall=np.array([[[0,10,-20],[0,14,20],[0,10,20]]],np.float32)
        geometry=np.concatenate((floor,wall));people=[dict(name='av-jacket-male',horizontal_radius=.4,animated_height=2)]
        actor='0 0 1 3 1 1 0 100 0 4 -5 10.008 -5 5 10.008 -5 5 10.008 5 -5 10.008 5 16 5 0 0 0 0 0 -1 1'
        graph=build_graph(geometry.min(1),geometry.max(1),np.zeros((20,20)),4,['LRSCENE5 566 1 0 1 1 synthetic','base.lrm',actor],people,566,geometry)
        self.assertGreater(len(graph['edges']),0)
        self.assertTrue(all(abs(n['point'][1]-10.008)<.001 for n in graph['nodes']))
        for e in graph['edges']:
            a,b=graph['nodes'][e['a']]['point'],graph['nodes'][e['b']]['point']
            self.assertGreater(a[0]*b[0],0,'wall-crossing edge must be rejected')

    def test_omni_entertainment_is_not_a_neutral_palette(self):
        self.assertEqual(city_faction({'cities':{'705':{'theme':'entertainment'}}},705),'omni')

    def test_large_medium_adds_staggered_fixed_period_instead_of_retiming(self):
        route=dict(start=[80,160,80],end=[240,160,80],speed=8)
        lines=['LRSCENE5 566 1 0 1 0 synthetic-routes','base.lrm','0','-']
        result,names=extend(lines,[],[],dict(name='large'),dict(routes={'large':route}),dict(cities={}),566)
        actors=[line.split() for line in result[1+len(names):3+len(names)]]
        self.assertEqual([a[2] for a in actors],['1','2'])
        self.assertEqual([a[-8] for a in actors],['900','900'])
        self.assertEqual(abs(float(actors[0][6])-float(actors[1][6])),450)

    def test_original_static_hull_alpha_policy(self):
        material={'fields':{'delta_state':(0,struct.pack('<i',0))}}
        def states(items):return [{'fields':{name:(0,struct.pack('<'+'i'*len(items),*(p[i] for p in items))) for i,name in enumerate(('rst_type','rst_value'))}}]
        self.assertTrue(opaque_material(material,states([(27,0),(29,1)])))
        self.assertFalse(opaque_material(material,states([(27,1)])))
        self.assertFalse(opaque_material(material,states([(15,1)])))
    def test_native_terrain_diagonal(self):
        h=np.array([[0,0],[0,1]],np.float32)
        t=terrain_triangles(h,4,np.array([1,40,1]),np.array([3,40,3]),1)
        self.assertEqual(t.shape,(2,3,3))
        self.assertEqual(float(t[0,:,1].max()),0) # native default b--d diagonal
        h=np.array([[0,20],[0,1]],np.float32)
        t=terrain_triangles(h,4,np.array([1,40,1]),np.array([3,40,3]),1)
        self.assertEqual(float(t[1,:,1].max()),1) # steep cell selects a--c

    def test_swept_envelope_middle_and_rotated_width(self):
        t=np.array([[[50,30,3],[50,50,3],[51,40,3]]],float)
        self.assertTrue(possible_collision(t,np.array([0,40,0]),np.array([100,40,0]),4))
        self.assertFalse(possible_collision(t,np.array([0,40,0]),np.array([100,40,0]),2))

    def test_variety_preserves_nonpeople_routes_and_parent_indices(self):
        names=[f'old-{i}.lrm' for i in range(16)]
        person='0 0 1 3 1 1 0 100 0 4 0 0 0 10 0 0 10 0 10 0 0 10 16 5 0 0 0 0 0 -1 0'
        drone='1 9 1 0 1 0 0 100 0 1 5 4 5 16 5 0 0 0 0 0 -1 0'
        child='4 13 1 0 1 0 0 100 0 1 0 1 0 16 5 0 0 0 0 0 1 0'
        lines=['LRSCENE5 566 1 0 16 3 synthetic-original-routes',*names,person,drone,child,'0','ground-566.lrg']
        people=[dict(name=f'new-{i}',role=i%4,factions='all') for i in range(12)]
        result,meshes=extend(lines,people,[],None,{'appearance_clearance':{'0':[p['name'] for p in people]}},dict(cities={}),566,True)
        self.assertEqual(len(meshes),19)
        rows=result[20:23]
        self.assertEqual(rows[1].split()[1],'12')
        self.assertEqual(rows[1].split()[2:],drone.split()[2:])
        self.assertEqual(rows[2].split()[-2],'1')

    def test_unknown_faction_cannot_gain_omni_guard(self):
        people=[dict(name=f'new-{i}',role=0,factions='all') for i in range(11)]
        people.append(dict(name='omni-guard',role=3,factions='omni'))
        with self.assertRaisesRegex(ValueError,'12 compatible'):
            extend(['LRSCENE5 566 1 0 9 0 synthetic-routes',*[f'a{i}.lrm' for i in range(9)],'0','-'],people,[],None,{},dict(cities={}),566,True)

if __name__=='__main__':unittest.main()
