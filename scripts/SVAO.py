from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('VAO', 'VAO', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 8, 'SVAOInputMode': True})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('RTStochasticDepth', 'RTStochasticDepth', {'resolutionDivisor': 1})
    g.create_pass('NormalsToViewSpace', 'NormalsToViewSpace', {})
    g.create_pass('LinearizeDepth', 'LinearizeDepth', {})
    g.create_pass('SVAO', 'SVAO', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 8})
    g.add_edge('LinearizeDepth.linearDepthOut', 'VAO.linearDepthIn')
    g.add_edge('GBufferRaster.depth', 'LinearizeDepth.depthIn')
    g.add_edge('GBufferRaster.normW', 'NormalsToViewSpace.normalsWorldIn')
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'VAO.normalViewIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'SVAO.linearDepthIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'RTStochasticDepth.linearDepthIn')
    g.add_edge('RTStochasticDepth.stochasticDepth', 'SVAO.stochDepthIn')
    g.add_edge('VAO.aoOut', 'SVAO.aoInOut')
    g.add_edge('VAO.aoMaskOut', 'SVAO.aoMaskIn')
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'SVAO.normalViewIn')
    g.mark_output('SVAO.aoInOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
