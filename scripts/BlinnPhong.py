from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_g():
    g = RenderGraph('g')
    g.create_pass('Blinn-Phong', 'Blinn-Phong', {'gAmbientLight': [0.20000000298023224, 0.20000000298023224, 0.20000000298023224]})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.add_edge('GBufferRaster.diffuseOpacity', 'Blinn-Phong.diffuseOpacity')
    g.add_edge('GBufferRaster.specRough', 'Blinn-Phong.specRough')
    g.add_edge('GBufferRaster.normW', 'Blinn-Phong.normalBuffer')
    g.mark_output('Blinn-Phong.diffuseOpacity')
    return g

g = render_graph_g()
try: m.addGraph(g)
except NameError: None
