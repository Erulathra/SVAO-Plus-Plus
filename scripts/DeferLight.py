from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('DeferredLighting', 'DeferredLighting', {})
    g.add_edge('GBufferRaster.posW', 'DeferredLighting.posW')
    g.add_edge('GBufferRaster.normW', 'DeferredLighting.normW')
    g.add_edge('GBufferRaster.diffuseOpacity', 'DeferredLighting.diffuseOpacity')
    g.add_edge('GBufferRaster.specRough', 'DeferredLighting.specRough')
    g.mark_output('DeferredLighting.kColorOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
