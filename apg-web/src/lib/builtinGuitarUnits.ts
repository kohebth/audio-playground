import autoWahUnitYaml from '../../../test/fixtures/units-v2/auto_wah.unit.v2.yaml?raw';
import freezeLooperUnitYaml from '../../../test/fixtures/units-v2/freeze_looper.unit.v2.yaml?raw';
import octaveShifterUnitYaml from '../../../test/fixtures/units-v2/octave_shifter.unit.v2.yaml?raw';
import stereoEnhancerUnitYaml from '../../../test/fixtures/units-v2/stereo_enhancer.unit.v2.yaml?raw';
import type { WorkspaceFile } from './backendSamples';

export const builtinGuitarUnits: WorkspaceFile[] = [
  {
    path: 'test/fixtures/units-v2/auto_wah.unit.v2.yaml',
    role: 'unit',
    content: autoWahUnitYaml,
    originalContent: autoWahUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/octave_shifter.unit.v2.yaml',
    role: 'unit',
    content: octaveShifterUnitYaml,
    originalContent: octaveShifterUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/stereo_enhancer.unit.v2.yaml',
    role: 'unit',
    content: stereoEnhancerUnitYaml,
    originalContent: stereoEnhancerUnitYaml,
  },
  {
    path: 'test/fixtures/units-v2/freeze_looper.unit.v2.yaml',
    role: 'unit',
    content: freezeLooperUnitYaml,
    originalContent: freezeLooperUnitYaml,
  },
];
