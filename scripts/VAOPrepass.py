from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('NormalsToViewSpace', 'NormalsToViewSpace', {})
    g.create_pass('GBufferLite', 'GBufferLite', {})
    g.create_pass('VAOPrepass', 'VAOPrepass', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 8, 'resolutionDivisor': 4, 'enableGuardBand': True})
    g.add_edge('GBufferLite.normW', 'NormalsToViewSpace.normalsWorldIn')
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'VAOPrepass.normalViewIn')
    g.add_edge('GBufferLite.linearDepth', 'VAOPrepass.linearDepthIn')
    g.mark_output('VAOPrepass.aoMaskOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
