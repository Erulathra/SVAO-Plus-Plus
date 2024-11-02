from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('SSAO', 'SSAO', {'radius': 0.2, 'bias': 0.0})
    g.create_pass('StochasticDepthStratfield', 'StochasticDepthStratfield', {'alpha': 0.1, 'numSamples': 8})
    g.add_edge('GBufferRaster.depth', 'SSAO.depth')
    g.add_edge('GBufferRaster.normW', 'SSAO.normalBuffer')
    g.add_edge('GBufferRaster.depth', 'StochasticDepthStratfield.depthTexture')
    g.add_edge('GBufferRaster', 'StochasticDepthStratfield')
    g.add_edge('StochasticDepthStratfield.stochasticDepth', 'SSAO.stochasticDepth')
    g.mark_output('SSAO.ambientOcclusionMask')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
