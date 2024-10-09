from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('GBufferLite', 'GBufferLite', {})
    g.create_pass('Blinn-Phong', 'Blinn-Phong', {'gAmbientLight': [0.20000000298023224, 0.20000000298023224, 0.20000000298023224]})
    g.add_edge('GBufferLite.specRough', 'Blinn-Phong.specRough')
    g.add_edge('GBufferLite.diffuseOpacity', 'Blinn-Phong.diffuseOpacity')
    g.add_edge('GBufferLite.normW', 'Blinn-Phong.normalBuffer')
    g.mark_output('Blinn-Phong.diffuseOpacity')
    g.mark_output('GBufferLite.diffuseOpacity')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
