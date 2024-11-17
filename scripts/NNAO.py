from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('StochasticDepthStratfield', 'StochasticDepthStratfield', {'alpha': 0.10000000149011612, 'numSamples': 8})
    g.create_pass('NNAO', 'NNAO', {'bias': 0.0, 'radius': 0.20000000298023224, 'enableSDF': True})
    g.add_edge('GBufferRaster.depth', 'StochasticDepthStratfield.depthTexture')
    g.add_edge('GBufferRaster', 'StochasticDepthStratfield')
    g.add_edge('GBufferRaster.depth', 'NNAO.depth')
    g.add_edge('GBufferRaster.normW', 'NNAO.normalBuffer')
    g.add_edge('StochasticDepthStratfield.stochasticDepth', 'NNAO.stochasticDepth')
    g.mark_output('NNAO.ambientOcclusionMask')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
