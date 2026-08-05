# GameXXK Shared Currency Strip Rollout

## Scope

Use the approved `02_城镇HUD` currency presentation as the single visual reference for every remaining out-of-run page that currently shows the long top currency strip.

Target these eight top-level PSD pages:

- `03_主角背包`
- `04_伙伴编队`
- `05_图鉴`
- `06_任务日志`
- `07_商店交易`
- `13_主角背包_物品选中`
- `14_伙伴编队_角色选中`
- `15_图鉴_怪物选中`

`02_城镇HUD` is already the approved reference and is not rebuilt. In-run route, event, battle, targeting, and reward pages are outside this pass and retain copper-coin presentation.

## Shared Top Currency Component

- Reuse the exact 320 x 86 compact paper derivative already approved on `02_城镇HUD`.
- Place it at page-local `[1570, 28, 320, 86]` on every target page.
- Use `resource_gold.png` as the out-of-run ingot icon.
- Keep the icon and editable numeric value centered as one unit, matching `02_城镇HUD`.
- Do not add a written `元宝` label or a second currency to the strip.
- Hide and preserve every replaced long strip, copper icon, and value layer as legacy content; do not delete user-tuned layers.

## Shop Currency Rule

`07_商店交易` is fully out-of-run. Replace copper-coin icons with the same ingot icon in all visible and hidden shop states:

- top balance;
- six equipment-pack prices;
- companion-pack price;
- purchase button price;
- insufficient-funds state;
- confirmation state;
- purchase-result state.

Replace every shop-layer occurrence sourced from `resource_coin.png`, including hidden state layers. Keep all existing price values and editable text unchanged. Only the currency icon identity changes.

## Isolation

- Mutate only the eight target pages inside `GameXXK_UI_Master_V1.psd`.
- Do not rebuild the whole UI Master pipeline.
- Do not modify `01_主菜单`, the approved `02_城镇HUD`, in-run pages, `RuntimeAssets`, or Unreal Engine assets.
- Preserve the current page layout, user-tuned content, equipment/item content, navigation icons, hero identity, and interaction states.

## Validation

- Export one 1920 x 1080 review PNG per target page.
- Confirm every target page uses the same top-strip box and ingot icon source.
- Confirm every shop price icon is an ingot and no shop price uses a copper-coin icon.
- Confirm all editable price values remain unchanged.
- Confirm the master PSD retains 18 top-level pages.
- Confirm non-target page signatures, including `01_主菜单` and `02_城镇HUD`, remain unchanged.
- This is PSD/art consolidation; visual and structural verification replace TDD.
