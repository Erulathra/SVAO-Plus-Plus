from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('RTStochasticDepth', 'RTStochasticDepth', {'resolutionDivisor': 1})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('LinearizeDepth', 'LinearizeDepth', {})
    g.add_edge('GBufferRaster.depth', 'LinearizeDepth.depthIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'RTStochasticDepth.linearDepthIn')
    g.mark_output('RTStochasticDepth.stochasticDepth')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
