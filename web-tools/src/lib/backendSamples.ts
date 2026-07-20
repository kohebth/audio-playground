import atomCatalogRaw from '../../../test/golden/v2-inspect-atoms.json?raw';
import atomCatalogManifestRaw from '../../../test/golden/v2-inspect-atoms.manifest.txt?raw';
import chorusUnitYaml from '../../../test/fixtures/units-v2/chorus.unit.v2.yaml?raw';
import delayUnitYaml from '../../../test/fixtures/units-v2/delay.unit.v2.yaml?raw';
import noiseGateUnitYaml from '../../../test/fixtures/units-v2/noise_gate.unit.v2.yaml?raw';
import overdriveUnitYaml from '../../../test/fixtures/units-v2/overdrive.unit.v2.yaml?raw';
import phaserUnitYaml from '../../../test/fixtures/units-v2/phaser.unit.v2.yaml?raw';
import projectYaml from '../../../test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml?raw';
import projectInspectRaw from '../../../test/golden/v2-inspect-project-guitar-pedalboard.json?raw';
import projectRenderRaw from '../../../test/golden/v2-render-project-guitar-pedalboard.json?raw';
import projectValidationRaw from '../../../test/golden/v2-validate-project-guitar-pedalboard.json?raw';
import schroederReverbUnitYaml from '../../../test/fixtures/units-v2/schroeder_reverb.unit.v2.yaml?raw';
import toneStackUnitYaml from '../../../test/fixtures/units-v2/tone_stack.unit.v2.yaml?raw';
import tremoloUnitYaml from '../../../test/fixtures/units-v2/tremolo.unit.v2.yaml?raw';
import wetDryMixUnitYaml from '../../../test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml?raw';
import unitInspectRaw from '../../../test/golden/v2-inspect-unit-simple_gain.json?raw';

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

export type ProjectScene = {
  name: string;
  params: ProjectParam[];
  bypass: Record<string, boolean>;
};

export type ProjectInspect = {
  schema: string;
  file: string;
  name: string;
  version: string;
  units: ProjectUnit[];
  nodes: ProjectInstance[];
  routes: ProjectRoute[];
  scenes: ProjectScene[];
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
  benchmarkProject: string;
  exportWasm: string;
  exportM7: string;
};

export type AtomCatalogField = {
  name: string;
  type: string;
  buffer_samples?: number;
  required?: boolean;
  default?: number | boolean | string | number[] | number[][];
  min?: number;
  max?: number;
  unit?: 'hz' | 'ms' | 'db' | 'ratio' | 'samples';
  scale?: 'linear' | 'logarithmic';
  realtime?: boolean;
  smoothing_ms?: number;
  structural?: boolean;
  options?: string[];
  option_values?: number[];
};

export type AtomCatalogAtom = {
  name: string;
  category: string;
  visibility: 'public' | 'advanced' | 'internal';
  dispatch: string;
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
    './build/apg-v2 validate project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml',
  renderProject:
    './build/apg-v2 render project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml',
  benchmarkProject:
    './build/apg-v2 benchmark project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml',
  exportWasm:
    './build/apg-v2 export --target wasm_realtime test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml dist/web/',
  exportM7:
    './build/apg-v2 export --target m7_static test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml build/m7/',
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
    path: 'test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml',
    role: 'project',
    content: projectYaml,
    originalContent: projectYaml,
  },
  {
    path: 'test/fixtures/units-v2/noise_gate.unit.v2.yaml',
    role: 'unit',
    content: noiseGateUnitYaml,
    originalContent: noiseGateUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/phaser.unit.v2.yaml',
    role: 'unit',
    content: phaserUnitYaml,
    originalContent: phaserUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/overdrive.unit.v2.yaml',
    role: 'unit',
    content: overdriveUnitYaml,
    originalContent: overdriveUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/tone_stack.unit.v2.yaml',
    role: 'unit',
    content: toneStackUnitYaml,
    originalContent: toneStackUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/tremolo.unit.v2.yaml',
    role: 'unit',
    content: tremoloUnitYaml,
    originalContent: tremoloUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/chorus.unit.v2.yaml',
    role: 'unit',
    content: chorusUnitYaml,
    originalContent: chorusUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/delay.unit.v2.yaml',
    role: 'unit',
    content: delayUnitYaml,
    originalContent: delayUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/schroeder_reverb.unit.v2.yaml',
    role: 'unit',
    content: schroederReverbUnitYaml,
    originalContent: schroederReverbUnitYaml,
  },
];

export const wetDryMixWorkspaceFile: WorkspaceFile = {
  path: 'test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml',
  role: 'unit',
  content: wetDryMixUnitYaml,
  originalContent: wetDryMixUnitYaml,
};
