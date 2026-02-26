/**
 * EvidenceCollectionForm – SolidJS component
 *
 * Renders a forensic evidence collection form.  When the examiner picks a
 * Device Type the Interface and Protocol dropdowns automatically repopulate
 * with options that are common for that device type.
 *
 * Each dropdown always ends with "Other (specify below)" so the examiner can
 * type in a custom value that is not in the predefined list.
 *
 * Usage (in your Tauri SolidJS app):
 *   import { EvidenceCollectionForm } from './components/EvidenceCollectionForm';
 *   <EvidenceCollectionForm onSubmit={(data) => console.log(data)} />
 */

import { createSignal, createMemo, For, Show, Component } from "solid-js";
import {
  DEVICE_TYPE_CONFIGS,
  DeviceTypeConfig,
  FormOption,
  getDeviceTypeConfig,
} from "../deviceTypeData";

// ---------------------------------------------------------------------------
// Form data shape
// ---------------------------------------------------------------------------

export interface EvidenceFormData {
  // Case metadata
  caseNumber: string;
  examiner: string;
  dateCollected: string;

  // Device identification
  deviceType: string;
  deviceTypeOther: string;
  make: string;
  model: string;
  serialNumber: string;
  assetTag: string;

  // Acquisition settings
  interface: string;
  interfaceOther: string;
  protocol: string;
  protocolOther: string;
  writeBlockerUsed: boolean;
  acquisitionTool: string;

  // Hashes / integrity
  md5Hash: string;
  sha256Hash: string;

  // Chain of custody
  collectedFrom: string;
  storageLocation: string;
  notes: string;
}

const emptyForm = (): EvidenceFormData => ({
  caseNumber: "",
  examiner: "",
  dateCollected: new Date().toISOString().split("T")[0],
  deviceType: "",
  deviceTypeOther: "",
  make: "",
  model: "",
  serialNumber: "",
  assetTag: "",
  interface: "",
  interfaceOther: "",
  protocol: "",
  protocolOther: "",
  writeBlockerUsed: false,
  acquisitionTool: "",
  md5Hash: "",
  sha256Hash: "",
  collectedFrom: "",
  storageLocation: "",
  notes: "",
});

// ---------------------------------------------------------------------------
// Helper sub-component: dynamic dropdown with optional "Other" free-text
// ---------------------------------------------------------------------------

interface DynamicSelectProps {
  id: string;
  label: string;
  options: FormOption[];
  value: string;
  otherValue: string;
  required?: boolean;
  onSelect: (value: string) => void;
  onOtherChange: (text: string) => void;
}

const DynamicSelect: Component<DynamicSelectProps> = (props) => {
  const selectedOption = createMemo(() =>
    props.options.find((o) => o.value === props.value)
  );
  const showOther = createMemo(() => selectedOption()?.is_other === true);

  return (
    <div class="form-group">
      <label for={props.id}>
        {props.label}
        {props.required && <span class="required-mark">*</span>}
      </label>
      <select
        id={props.id}
        value={props.value}
        required={props.required}
        onChange={(e) => props.onSelect(e.currentTarget.value)}
      >
        <option value="">— Select —</option>
        <For each={props.options}>
          {(opt) => (
            <option value={opt.value}>{opt.label}</option>
          )}
        </For>
      </select>
      <Show when={showOther()}>
        <input
          type="text"
          placeholder={`Specify ${props.label.toLowerCase()}…`}
          value={props.otherValue}
          onInput={(e) => props.onOtherChange(e.currentTarget.value)}
          class="other-input"
          aria-label={`Custom ${props.label}`}
        />
      </Show>
    </div>
  );
};

// ---------------------------------------------------------------------------
// Main component
// ---------------------------------------------------------------------------

interface Props {
  /** Called when the form is submitted with valid data */
  onSubmit?: (data: EvidenceFormData) => void;
  /** Initial form values (optional) */
  initialData?: Partial<EvidenceFormData>;
}

export const EvidenceCollectionForm: Component<Props> = (props) => {
  const [form, setForm] = createSignal<EvidenceFormData>({
    ...emptyForm(),
    ...(props.initialData ?? {}),
  });

  // Derive the currently-selected device-type config (undefined = none chosen)
  const deviceConfig = createMemo<DeviceTypeConfig | undefined>(() =>
    getDeviceTypeConfig(form().deviceType)
  );

  // When the device type changes reset interface / protocol selections so
  // stale values from the previous device type are not submitted.
  const handleDeviceTypeChange = (value: string) => {
    setForm((f) => ({
      ...f,
      deviceType: value,
      deviceTypeOther: "",
      interface: "",
      interfaceOther: "",
      protocol: "",
      protocolOther: "",
    }));
  };

  const set = <K extends keyof EvidenceFormData>(
    key: K,
    value: EvidenceFormData[K]
  ) => setForm((f) => ({ ...f, [key]: value }));

  const handleSubmit = (e: SubmitEvent) => {
    e.preventDefault();
    props.onSubmit?.(form());
  };

  // ----- "device type" dropdown options derived from DEVICE_TYPE_CONFIGS -----
  const deviceTypeOptions: FormOption[] = DEVICE_TYPE_CONFIGS.map((c) => ({
    value: c.value,
    label: c.label,
    is_other: c.value === "other_device",
  }));

  return (
    <form class="evidence-form" onSubmit={handleSubmit} noValidate>
      <h2 class="form-title">Forensic Evidence Collection Form</h2>

      {/* ── Case Metadata ─────────────────────────────────────────────────── */}
      <fieldset>
        <legend>Case Information</legend>
        <div class="form-row">
          <div class="form-group">
            <label for="caseNumber">Case Number<span class="required-mark">*</span></label>
            <input
              id="caseNumber"
              type="text"
              value={form().caseNumber}
              required
              onInput={(e) => set("caseNumber", e.currentTarget.value)}
            />
          </div>
          <div class="form-group">
            <label for="examiner">Examiner Name<span class="required-mark">*</span></label>
            <input
              id="examiner"
              type="text"
              value={form().examiner}
              required
              onInput={(e) => set("examiner", e.currentTarget.value)}
            />
          </div>
          <div class="form-group">
            <label for="dateCollected">Date Collected<span class="required-mark">*</span></label>
            <input
              id="dateCollected"
              type="date"
              value={form().dateCollected}
              required
              onInput={(e) => set("dateCollected", e.currentTarget.value)}
            />
          </div>
        </div>
      </fieldset>

      {/* ── Device Identification ─────────────────────────────────────────── */}
      <fieldset>
        <legend>Device Identification</legend>

        {/* Device Type – drives the Interface and Protocol dropdowns */}
        <DynamicSelect
          id="deviceType"
          label="Device Type"
          options={deviceTypeOptions}
          value={form().deviceType}
          otherValue={form().deviceTypeOther}
          required
          onSelect={handleDeviceTypeChange}
          onOtherChange={(v) => set("deviceTypeOther", v)}
        />

        <div class="form-row">
          <div class="form-group">
            <label for="make">Make / Manufacturer</label>
            <input
              id="make"
              type="text"
              value={form().make}
              onInput={(e) => set("make", e.currentTarget.value)}
            />
          </div>
          <div class="form-group">
            <label for="model">Model</label>
            <input
              id="model"
              type="text"
              value={form().model}
              onInput={(e) => set("model", e.currentTarget.value)}
            />
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label for="serialNumber">Serial Number</label>
            <input
              id="serialNumber"
              type="text"
              value={form().serialNumber}
              onInput={(e) => set("serialNumber", e.currentTarget.value)}
            />
          </div>
          <div class="form-group">
            <label for="assetTag">Asset / Evidence Tag</label>
            <input
              id="assetTag"
              type="text"
              value={form().assetTag}
              onInput={(e) => set("assetTag", e.currentTarget.value)}
            />
          </div>
        </div>
      </fieldset>

      {/* ── Acquisition Settings (dynamic based on device type) ───────────── */}
      <fieldset>
        <legend>Acquisition Settings</legend>

        <Show
          when={deviceConfig()}
          fallback={
            <p class="hint">
              Select a Device Type above to see relevant interface and protocol
              options.
            </p>
          }
        >
          {(config) => (
            <>
              <DynamicSelect
                id="interface"
                label="Acquisition Interface"
                options={config().interfaces}
                value={form().interface}
                otherValue={form().interfaceOther}
                required
                onSelect={(v) => set("interface", v)}
                onOtherChange={(v) => set("interfaceOther", v)}
              />

              <DynamicSelect
                id="protocol"
                label="Acquisition Protocol / Method"
                options={config().protocols}
                value={form().protocol}
                otherValue={form().protocolOther}
                required
                onSelect={(v) => set("protocol", v)}
                onOtherChange={(v) => set("protocolOther", v)}
              />
            </>
          )}
        </Show>

        <div class="form-group form-group--checkbox">
          <label>
            <input
              type="checkbox"
              checked={form().writeBlockerUsed}
              onChange={(e) => set("writeBlockerUsed", e.currentTarget.checked)}
            />
            Hardware Write-Blocker used
          </label>
        </div>

        <div class="form-group">
          <label for="acquisitionTool">Acquisition Tool / Software</label>
          <input
            id="acquisitionTool"
            type="text"
            placeholder="e.g. FTK Imager 4.7, dd, Cellebrite UFED…"
            value={form().acquisitionTool}
            onInput={(e) => set("acquisitionTool", e.currentTarget.value)}
          />
        </div>
      </fieldset>

      {/* ── Hash / Integrity ─────────────────────────────────────────────── */}
      <fieldset>
        <legend>Hash Verification</legend>
        <div class="form-row">
          <div class="form-group">
            <label for="md5Hash">MD5 Hash</label>
            <input
              id="md5Hash"
              type="text"
              placeholder="32 hex characters"
              maxLength={32}
              value={form().md5Hash}
              onInput={(e) => set("md5Hash", e.currentTarget.value)}
            />
          </div>
          <div class="form-group">
            <label for="sha256Hash">SHA-256 Hash</label>
            <input
              id="sha256Hash"
              type="text"
              placeholder="64 hex characters"
              maxLength={64}
              value={form().sha256Hash}
              onInput={(e) => set("sha256Hash", e.currentTarget.value)}
            />
          </div>
        </div>
      </fieldset>

      {/* ── Chain of Custody ─────────────────────────────────────────────── */}
      <fieldset>
        <legend>Chain of Custody</legend>
        <div class="form-row">
          <div class="form-group">
            <label for="collectedFrom">Collected From (person / location)</label>
            <input
              id="collectedFrom"
              type="text"
              value={form().collectedFrom}
              onInput={(e) => set("collectedFrom", e.currentTarget.value)}
            />
          </div>
          <div class="form-group">
            <label for="storageLocation">Current Storage Location</label>
            <input
              id="storageLocation"
              type="text"
              value={form().storageLocation}
              onInput={(e) => set("storageLocation", e.currentTarget.value)}
            />
          </div>
        </div>
        <div class="form-group">
          <label for="notes">Additional Notes</label>
          <textarea
            id="notes"
            rows={4}
            value={form().notes}
            onInput={(e) => set("notes", e.currentTarget.value)}
          />
        </div>
      </fieldset>

      {/* ── Actions ──────────────────────────────────────────────────────── */}
      <div class="form-actions">
        <button type="button" class="btn btn--secondary" onClick={() => setForm(emptyForm())}>
          Clear Form
        </button>
        <button type="submit" class="btn btn--primary">
          Save Evidence Record
        </button>
      </div>
    </form>
  );
};

export default EvidenceCollectionForm;
