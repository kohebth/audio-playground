import atomCatalogManifestRaw from '../../../../test/golden/v2-inspect-atoms.manifest.txt?raw';
import projectInspectRaw from '../../../../test/golden/v2-inspect-project-guitar-pedalboard.json?raw';
import projectRenderRaw from '../../../../test/golden/v2-render-project-guitar-pedalboard.json?raw';
import projectValidationRaw from '../../../../test/golden/v2-validate-project-guitar-pedalboard.json?raw';
import unitInspectRaw from '../../../../test/golden/v2-inspect-unit-simple_gain.json?raw';

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

export type UnitInspect = {
  schema: string;
  file: string;
  name: string;
  version: string;
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
};

function parseJson<T>(raw: string): T {
  return JSON.parse(raw) as T;
}

function parseManifest(raw: string): Record<string, string> {
  return Object.fromEntries(raw.trim().split('\n').map(line => line.split('='))) as Record<string, string>;
}

export const backendSamples = {
  project: parseJson<ProjectInspect>(projectInspectRaw),
  validation: parseJson<ValidationResult>(projectValidationRaw),
  render: parseJson<RenderResult>(projectRenderRaw),
  unit: parseJson<UnitInspect>(unitInspectRaw),
  atomCatalog: parseManifest(atomCatalogManifestRaw),
};

export const sampleSources = {
  project: 'test/golden/v2-inspect-project-guitar-pedalboard.json',
  validation: 'test/golden/v2-validate-project-guitar-pedalboard.json',
  render: 'test/golden/v2-render-project-guitar-pedalboard.json',
  unit: 'test/golden/v2-inspect-unit-simple_gain.json',
  atomCatalog: 'test/golden/v2-inspect-atoms.manifest.txt',
};
