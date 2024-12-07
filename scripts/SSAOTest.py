from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('SSAO', 'SSAO', {'bias': 0.0, 'radius': 0.20000000298023224, 'enableSDF': True, 'showProblematicSamples': False})
    g.create_pass('RTStochasticDepth', 'RTStochasticDepth', {'resolutionDivisor': 1})
    g.create_pass('LinearizeDepth', 'LinearizeDepth', {})
    g.add_edge('RTStochasticDepth.stochasticDepth', 'SSAO.stochasticDepth')
    g.add_edge('GBufferRaster.depth', 'SSAO.depth')
    g.add_edge('GBufferRaster.normW', 'SSAO.normalBuffer')
    g.add_edge('LinearizeDepth.linearDepthOut', 'RTStochasticDepth.linearDepthIn')
    g.add_edge('GBufferRaster.depth', 'LinearizeDepth.depthIn')
    g.mark_output('SSAO.ambientOcclusionMask')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
