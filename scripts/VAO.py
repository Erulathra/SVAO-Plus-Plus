from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('VAO', 'VAO', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 8, 'SVAOInputMode': False})
    g.create_pass('NormalsToViewSpace', 'NormalsToViewSpace', {})
    g.create_pass('LinearizeDepth', 'LinearizeDepth', {})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'VAO.normalViewIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'VAO.linearDepthIn')
    g.add_edge('GBufferRaster.depth', 'LinearizeDepth.depthIn')
    g.add_edge('GBufferRaster.normW', 'NormalsToViewSpace.normalsWorldIn')
    g.mark_output('VAO.aoOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None