// Review prototype only. A fresh Ctrl press toggles help for the active preview.
function interactivePreview(card, variant) {
  return `<details class="interaction-demo"><summary>交互预览：Shift详述 · Ctrl开关pill说明</summary>
  <div class="interactive-preview" tabindex="0" data-card-id="${esc(card.id)}" data-quality="${esc(variant.quality)}">
    <div class="preview-card-name">${esc(card.name)}</div>
    <div class="interaction-mode">简述</div><div class="interaction-content">
      <div class="target-line">${esc(variant.target_heading)}</div><div class="text">${copy(variant.compact)}</div>
    </div>
  </div></details>`;
}

(() => {
  let active = null;
  let shift = false;
  let control = false;
  let pillOpen = false;
  function paint(mode) {
    if (!active || !active.isConnected) return;
    const card = DATA.cards.find(c => c.id === active.dataset.cardId);
    const variant = card.variants.find(v => v.quality === active.dataset.quality);
    const content = active.querySelector('.interaction-content');
    active.querySelector('.interaction-mode').textContent = mode === 'pills' ? '本牌pill说明 · 再按Ctrl关闭' : mode === 'detail' ? '详述 · 松开Shift返回' : '简述';
    active.dataset.mode = mode;
    if (mode === 'pills') {
      content.innerHTML = variant.pill_descriptions.length
        ? variant.pill_descriptions.map(p => `<div class="pill-explanation"><strong>${esc(p.name)}</strong><div>${copy(p.description)}</div></div>`).join('')
        : '无额外关键词说明。';
      if (variant.pill_shared_note) content.innerHTML += `<div class="pill-shared-note">${esc(variant.pill_shared_note)}</div>`;
    } else {
      content.innerHTML = `<div class="target-line">${esc(variant.target_heading)}</div><div class="text">${copy(mode === 'detail' ? variant.detail : variant.compact)}</div>`;
    }
    content.scrollTop = 0;
  }
  function leave() {
    pillOpen = false;
    if (active) paint('compact');
    active = null;
  }
  function enter(box, shiftKey, ctrlKey) {
    if (!box || box === active) return;
    leave();
    active = box;
    shift = !!shiftKey;
    control = !!ctrlKey;
    paint(shift ? 'detail' : 'compact');
  }
  document.addEventListener('pointerover', e => enter(e.target.closest('.interactive-preview'), e.shiftKey, e.ctrlKey));
  document.addEventListener('pointerout', e => {
    if (active && active.contains(e.target) && !active.contains(e.relatedTarget)) leave();
  });
  document.addEventListener('focusin', e => enter(e.target.closest('.interactive-preview'), shift, control));
  document.addEventListener('focusout', e => {
    if (active && active.contains(e.target) && !active.contains(e.relatedTarget)) leave();
  });
  document.addEventListener('keydown', e => {
    if (e.key === 'Control') {
      if (active && !control && !e.repeat) {
        e.preventDefault();
        pillOpen = !pillOpen;
        paint(shift ? 'detail' : pillOpen ? 'pills' : 'compact');
      }
      control = true;
    }
    if (e.key === 'Shift' && !shift) { shift = true; paint('detail'); }
    if (e.key === 'Escape' && active) { pillOpen = false; paint(shift ? 'detail' : 'compact'); }
  });
  document.addEventListener('keyup', e => {
    if (e.key === 'Control') control = false;
    if (e.key === 'Shift') { shift = false; paint(pillOpen ? 'pills' : 'compact'); }
  });
  // Ctrl release retains the chosen mode. Mouse buttons retain their normal actions.
  window.addEventListener('blur', () => { shift = false; control = false; leave(); });
})();
