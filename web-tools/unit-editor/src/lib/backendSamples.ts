import atomCatalogRaw from '../../../../test/golden/v2-inspect-atoms.json?raw';
import atomCatalogManifestRaw from '../../../../test/golden/v2-inspect-atoms.manifest.txt?raw';
import delayUnitYaml from '../../../../units-v2/delay.unit.v2.yaml?raw';
import noiseGateUnitYaml from '../../../../units-v2/noise_gate.unit.v2.yaml?raw';
import overdriveUnitYaml from '../../../../units-v2/overdrive.unit.v2.yaml?raw';
import projectYaml from '../../../../projects-v2/guitar-pedalboard.project.v2.yaml?raw';
import projectInspectRaw from '../../../../test/golden/v2-inspect-project-guitar-pedalboard.json?raw';
import projectRenderRaw from '../../../../test/golden/v2-render-project-guitar-pedalboard.json?raw';
import projectValidationRaw from '../../../../test/golden/v2-validate-project-guitar-pedalboard.json?raw';
import toneStackUnitYaml from '../../../../units-v2/tone_stack.unit.v2.yaml?raw';
import tremoloUnitYaml from '../../../../units-v2/tremolo.unit.v2.yaml?raw';
import unitInspectRaw from '../../../../test/golden/v2-inspect-unit-simple_gain.json?raw';
import wetDryMixUnitYaml from '../../../../units-v2/wet_dry_mix.unit.v2.yaml?raw';

export type Compatibility = Record<string, boolean>;

export type ProjectUnit = {
  id: string;
  file: string;
  name: string;
  compatibility: Compatibility;
};

export type ProjectParam = {
  key: string;
  value: string;
};

export type ProjectInstance = {
  id: string;
  unit: string;
  params: ProjectParam[];
};

export type ProjectRoute = {
  from: string;
  to: string;
};

export type ProjectInspect = {
  schema: string;
  file: string;
  name: string;
  version: string;
  units: ProjectUnit[];
  nodes: ProjectInstance[];
  routes: ProjectRoute[];
  targets: {
    default: string;
    export: string[];
  };
  compiled: {
    params: number;
    signals: number;
    nodes: number;
    schedule: number;
  };
};

export type ValidationDiagnostic = {
  code?: string;
  file?: string;
  path?: string;
  message?: string;
};

export type ValidationResult = {
  schema: string;
  ok: boolean;
  file: string;
  errors: ValidationDiagnostic[];
  warnings: ValidationDiagnostic[];
};

export type RenderResult = {
  schema: string;
  ok: boolean;
  file: string;
  input: string;
  sample_rate: number;
  frames: number;
  output: {
    peak: number;
    rms: number;
    sum: number;
    samples: number[];
  };
};

export type BackendCommands = {
  validateProject: string;
  renderProject: string;
};

export type AtomCatalogField = {
  name: string;
  type: string;
  buffer_samples?: number;
};

export type AtomCatalogAtom = {
  name: string;
  category: string;
  stateful: boolean;
  profiles: Compatibility;
  inputs: AtomCatalogField[];
  outputs: AtomCatalogField[];
  config: AtomCatalogField[];
  state: AtomCatalogField[];
};

export type AtomCatalog = {
  schema: string;
  atoms: AtomCatalogAtom[];
};

export type UnitInspect = {
  schema: string;
  file: string;
  name: string;
  version: string;
  meta?: {
    title?: string;
    category?: string;
    description?: string;
  };
  compatibility?: Compatibility;
  params: Array<{
    name: string;
    type: string;
    default: string;
    min?: string;
    max?: string;
    smoothing_ms?: string;
    ui?: {
      label?: string;
      control?: string;
      unit?: string;
    };
  }>;
  ports: {
    inputs: Array<{
      name: string;
      type: string;
      channels?: string;
    }>;
    outputs: Array<{
      name: string;
      type: string;
      channels?: string;
    }>;
  };
  graph: {
    signals: string[];
    nodes: Array<{
      id: string;
      atom: string;
      bindings: Record<string, number>;
    }>;
  };
};

function parseJson<T>(raw: string): T {
  return JSON.parse(raw) as T;
}

function parseManifest(raw: string): Record<string, string> {
  return Object.fromEntries(raw.trim().split('\n').map(line => line.split('='))) as Record<string, string>;
}

export type BackendFixtureBundle = {
  project: ProjectInspect;
  validation: ValidationResult;
  render: RenderResult;
  unit: UnitInspect;
  atomCatalog: AtomCatalog;
  atomCatalogManifest: Record<string, string>;
};

export type WorkspaceFile = {
  path: string;
  role: 'project' | 'unit';
  content: string;
  originalContent: string;
};

export const backendSamples: BackendFixtureBundle = {
  project: parseJson<ProjectInspect>(projectInspectRaw),
  validation: parseJson<ValidationResult>(projectValidationRaw),
  render: parseJson<RenderResult>(projectRenderRaw),
  unit: parseJson<UnitInspect>(unitInspectRaw),
  atomCatalog: parseJson<AtomCatalog>(atomCatalogRaw),
  atomCatalogManifest: parseManifest(atomCatalogManifestRaw),
};

export const backendCommands: BackendCommands = {
  validateProject:
    '/tmp/audio-playground-apgcore-build/apg-v2 validate project projects-v2/guitar-pedalboard.project.v2.yaml',
  renderProject:
    '/tmp/audio-playground-apgcore-build/apg-v2 render project projects-v2/guitar-pedalboard.project.v2.yaml',
};

export const sampleSources = {
  project: 'test/golden/v2-inspect-project-guitar-pedalboard.json',
  validation: 'test/golden/v2-validate-project-guitar-pedalboard.json',
  render: 'test/golden/v2-render-project-guitar-pedalboard.json',
  unit: 'test/golden/v2-inspect-unit-simple_gain.json',
  atomCatalog: 'test/golden/v2-inspect-atoms.json',
  atomCatalogManifest: 'test/golden/v2-inspect-atoms.manifest.txt',
};

export const initialWorkspaceFiles: WorkspaceFile[] = [
  {
    path: 'projects-v2/guitar-pedalboard.project.v2.yaml',
    role: 'project',
    content: projectYaml,
    originalContent: projectYaml,
  },
  {
    path: 'units-v2/noise_gate.unit.v2.yaml',
    role: 'unit',
    content: noiseGateUnitYaml,
    originalContent: noiseGateUnitYaml,
  },
  {
    path: 'units-v2/overdrive.unit.v2.yaml',
    role: 'unit',
    content: overdriveUnitYaml,
    originalContent: overdriveUnitYaml,
  },
  {
    path: 'units-v2/tone_stack.unit.v2.yaml',
    role: 'unit',
    content: toneStackUnitYaml,
    originalContent: toneStackUnitYaml,
  },
  {
    path: 'units-v2/tremolo.unit.v2.yaml',
    role: 'unit',
    content: tremoloUnitYaml,
    originalContent: tremoloUnitYaml,
  },
  {
    path: 'units-v2/delay.unit.v2.yaml',
    role: 'unit',
    content: delayUnitYaml,
    originalContent: delayUnitYaml,
  },
  {
    path: 'units-v2/wet_dry_mix.unit.v2.yaml',
    role: 'unit',
    content: wetDryMixUnitYaml,
    originalContent: wetDryMixUnitYaml,
  },
];
