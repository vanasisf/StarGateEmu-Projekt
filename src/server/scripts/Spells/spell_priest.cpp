/*
 * Copyright (C) 2010-2012 Project StarGate
 */

#include "ScriptPCH.h"
#include "SpellAuraEffects.h"

enum PriestSpells
{
    PRIEST_SPELL_PENANCE_R1                      = 47540,
    PRIEST_SPELL_PENANCE_R1_DAMAGE               = 47758,
    PRIEST_SPELL_PENANCE_R1_HEAL                 = 47757,
};

// Guardian Spirit
class spell_pri_guardian_spirit : public SpellScriptLoader
{
public:
    spell_pri_guardian_spirit() : SpellScriptLoader("spell_pri_guardian_spirit") { }

    class spell_pri_guardian_spirit_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_pri_guardian_spirit_AuraScript);

        uint32 healPct;

        enum Spell
        {
            PRI_SPELL_GUARDIAN_SPIRIT_HEAL = 48153,
        };

        bool Validate(SpellEntry const * /*spellEntry*/)
        {
            return sSpellStore.LookupEntry(PRI_SPELL_GUARDIAN_SPIRIT_HEAL);
        }

        bool Load()
        {
            healPct = SpellMgr::CalculateSpellEffectAmount(GetSpellProto(), EFFECT_1);
            return true;
        }

        void CalculateAmount(AuraEffect const * /*aurEff*/, int32 & amount, bool & canBeRecalculated)
        {
            // Set absorbtion amount to unlimited
            amount = -1;
        }

        void Absorb(AuraEffect * aurEff, DamageInfo & dmgInfo, uint32 & absorbAmount)
        {
            Unit * target = GetTarget();
            if (dmgInfo.GetDamage() < target->GetHealth())
                return;

            int32 healAmount = int32(target->CountPctFromMaxHealth(healPct));
            // remove the aura now, we don't want 40% healing bonus
            Remove(AURA_REMOVE_BY_ENEMY_SPELL);
            target->CastCustomSpell(target, PRI_SPELL_GUARDIAN_SPIRIT_HEAL, &healAmount, NULL, NULL, true);
            absorbAmount = dmgInfo.GetDamage();
        }

        void Register()
        {
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_pri_guardian_spirit_AuraScript::CalculateAmount, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
            OnEffectAbsorb += AuraEffectAbsorbFn(spell_pri_guardian_spirit_AuraScript::Absorb, EFFECT_1);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_pri_guardian_spirit_AuraScript();
    }
};

class spell_pri_mana_burn : public SpellScriptLoader
{
    public:
        spell_pri_mana_burn() : SpellScriptLoader("spell_pri_mana_burn") { }

        class spell_pri_mana_burn_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_pri_mana_burn_SpellScript)
            bool Validate(SpellEntry const * /*spellEntry*/)
            {
                return true;
            }

            void HandleAfterHit()
            {
                Unit * unitTarget = GetHitUnit();
                if (!unitTarget)
                    return;

                unitTarget->RemoveAurasWithMechanic((1 << MECHANIC_FEAR) | (1 << MECHANIC_POLYMORPH));
            }

            void Register()
            {
                AfterHit += SpellHitFn(spell_pri_mana_burn_SpellScript::HandleAfterHit);
            }
        };

        SpellScript * GetSpellScript() const
        {
            return new spell_pri_mana_burn_SpellScript;
        }
};

class spell_pri_pain_and_suffering_proc : public SpellScriptLoader
{
    public:
        spell_pri_pain_and_suffering_proc() : SpellScriptLoader("spell_pri_pain_and_suffering_proc") { }

        // 47948 Pain and Suffering (proc)
        class spell_pri_pain_and_suffering_proc_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_pri_pain_and_suffering_proc_SpellScript)
            void HandleEffectScriptEffect(SpellEffIndex /*effIndex*/)
            {
                // Refresh Shadow Word: Pain on target
                if (Unit *unitTarget = GetHitUnit())
                    if (AuraEffect* aur = unitTarget->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_PRIEST, 0x8000, 0, 0, GetCaster()->GetGUID()))
                        aur->GetBase()->RefreshDuration();
            }

            void Register()
            {
                OnEffect += SpellEffectFn(spell_pri_pain_and_suffering_proc_SpellScript::HandleEffectScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript *GetSpellScript() const
        {
            return new spell_pri_pain_and_suffering_proc_SpellScript;
        }
};

class spell_pri_penance : public SpellScriptLoader
{
    public:
        spell_pri_penance() : SpellScriptLoader("spell_pri_penance") { }

        class spell_pri_penance_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_pri_penance_SpellScript)
            bool Validate(SpellEntry const * spellEntry)
            {
                if (!sSpellStore.LookupEntry(PRIEST_SPELL_PENANCE_R1))
                    return false;
                // can't use other spell than this penance due to spell_ranks dependency
                if (sSpellMgr->GetFirstSpellInChain(PRIEST_SPELL_PENANCE_R1) != sSpellMgr->GetFirstSpellInChain(spellEntry->Id))
                    return false;

                uint8 rank = sSpellMgr->GetSpellRank(spellEntry->Id);
                if (!sSpellMgr->GetSpellWithRank(PRIEST_SPELL_PENANCE_R1_DAMAGE, rank, true))
                    return false;
                if (!sSpellMgr->GetSpellWithRank(PRIEST_SPELL_PENANCE_R1_HEAL, rank, true))
                    return false;

                return true;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                Unit *unitTarget = GetHitUnit();
                if (!unitTarget || !unitTarget->isAlive())
                    return;

                Unit *caster = GetCaster();

                uint8 rank = sSpellMgr->GetSpellRank(GetSpellInfo()->Id);

                if (caster->IsFriendlyTo(unitTarget))
                    caster->CastSpell(unitTarget, sSpellMgr->GetSpellWithRank(PRIEST_SPELL_PENANCE_R1_HEAL, rank), false, 0);
                else
                    caster->CastSpell(unitTarget, sSpellMgr->GetSpellWithRank(PRIEST_SPELL_PENANCE_R1_DAMAGE, rank), false, 0);
            }

            void Register()
            {
                // add dummy effect spell handler to Penance
                OnEffect += SpellEffectFn(spell_pri_penance_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript *GetSpellScript() const
        {
            return new spell_pri_penance_SpellScript;
        }
};

// Reflective Shield
class spell_pri_reflective_shield_trigger : public SpellScriptLoader
{
public:
    spell_pri_reflective_shield_trigger() : SpellScriptLoader("spell_pri_reflective_shield_trigger") { }

    class spell_pri_reflective_shield_trigger_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_pri_reflective_shield_trigger_AuraScript);

        enum Spells
        {
            SPELL_PRI_REFLECTIVE_SHIELD_TRIGGERED = 33619,
            SPELL_PRI_REFLECTIVE_SHIELD_R1 = 33201,
        };

        bool Validate(SpellEntry const * /*spellEntry*/)
        {
            return sSpellStore.LookupEntry(SPELL_PRI_REFLECTIVE_SHIELD_TRIGGERED) && sSpellStore.LookupEntry(SPELL_PRI_REFLECTIVE_SHIELD_R1);
        }

        void Trigger(AuraEffect * aurEff, DamageInfo & dmgInfo, uint32 & absorbAmount)
        {
            Unit * target = GetTarget();
            if (dmgInfo.GetAttacker() == target)
                return;
            Unit * caster = GetCaster();
            if (!caster)
                return;
            if (AuraEffect * talentAurEff = target->GetAuraEffectOfRankedSpell(SPELL_PRI_REFLECTIVE_SHIELD_R1, EFFECT_0))
            {
                int32 bp = CalculatePctN(absorbAmount, talentAurEff->GetAmount());
                target->CastCustomSpell(dmgInfo.GetAttacker(), SPELL_PRI_REFLECTIVE_SHIELD_TRIGGERED, &bp, NULL, NULL, true, NULL, aurEff);
            }
        }

        void Register()
        {
             AfterEffectAbsorb += AuraEffectAbsorbFn(spell_pri_reflective_shield_trigger_AuraScript::Trigger, EFFECT_0);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_pri_reflective_shield_trigger_AuraScript();
    }
};

class spell_priest_flash_heal : public SpellScriptLoader
{
    public:
        spell_priest_flash_heal() : SpellScriptLoader("spell_priest_flash_heal") { }

        class spell_priest_flash_heal_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_priest_flash_heal_SpellScript)

            bool Validate(SpellEntry const * /*spellEntry*/)
            {
                return true;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Unit * caster = GetCaster())
                {
                    if (caster->GetTypeId() != TYPEID_PLAYER)
                        return;

                    caster->ToPlayer()->KilledMonsterCredit(44175, 0);
                }
            }

            void Register()
            {
                OnEffect += SpellEffectFn(spell_priest_flash_heal_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_HEAL);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_priest_flash_heal_SpellScript();
        }
};

void AddSC_priest_spell_scripts()
{
    new spell_pri_guardian_spirit();
    new spell_pri_mana_burn;
    new spell_pri_pain_and_suffering_proc;
    new spell_pri_penance;
    new spell_pri_reflective_shield_trigger();
    new spell_priest_flash_heal;
}