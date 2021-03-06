#include "GuiInstanceLoader_WorkflowCodegen.h"
#include "../../Reflection/TypeDescriptors/GuiReflectionEvents.h"
#include "../../Resources/GuiParserManager.h"

namespace vl
{
	namespace presentation
	{
		using namespace workflow;
		using namespace collections;
		using namespace reflection::description;

/***********************************************************************
WorkflowGenerateBindingVisitor
***********************************************************************/

		class WorkflowGenerateBindingVisitor : public Object, public GuiValueRepr::IVisitor
		{
		public:
			types::ResolvingResult&				resolvingResult;
			Ptr<WfBlockStatement>				statements;
			GuiResourceError::List&				errors;
			
			WorkflowGenerateBindingVisitor(types::ResolvingResult& _resolvingResult, Ptr<WfBlockStatement> _statements, GuiResourceError::List& _errors)
				:resolvingResult(_resolvingResult)
				, errors(_errors)
				, statements(_statements)
			{
			}

			void Visit(GuiTextRepr* repr)override
			{
			}

			void Visit(GuiAttSetterRepr* repr)override
			{
				IGuiInstanceLoader::TypeInfo reprTypeInfo;
				if (repr->instanceName != GlobalStringKey::Empty)
				{
					reprTypeInfo = resolvingResult.typeInfos[repr->instanceName];
				}
				
				if (reprTypeInfo.typeDescriptor && (reprTypeInfo.typeDescriptor->GetTypeDescriptorFlags() & TypeDescriptorFlags::ReferenceType) != TypeDescriptorFlags::Undefined)
				{
					WORKFLOW_ENVIRONMENT_VARIABLE_ADD

					FOREACH_INDEXER(Ptr<GuiAttSetterRepr::SetterValue>, setter, index, repr->setters.Values())
					{
						auto propertyName = repr->setters.Keys()[index];
						if (setter->binding != GlobalStringKey::Empty && setter->binding != GlobalStringKey::_Set)
						{
							if (auto binder = GetInstanceLoaderManager()->GetInstanceBinder(setter->binding))
							{
								auto propertyResolving = resolvingResult.propertyResolvings[setter->values[0].Obj()];
								if (propertyResolving.info->scope == GuiInstancePropertyInfo::Property)
								{
									WString expressionCode = setter->values[0].Cast<GuiTextRepr>()->text;
									auto instancePropertyInfo = reprTypeInfo.typeDescriptor->GetPropertyByName(propertyName.ToString(), true);

									if (instancePropertyInfo || !binder->RequirePropertyExist())
									{
										if (auto statement = binder->GenerateInstallStatement(
											resolvingResult,
											repr->instanceName,
											instancePropertyInfo,
											propertyResolving.loader,
											propertyResolving.propertyInfo,
											propertyResolving.info,
											expressionCode,
											setter->values[0]->tagPosition,
											errors))
										{
											statements->statements.Add(statement);
										}
									}
									else
									{
										errors.Add(GuiResourceError({ resolvingResult.resource }, setter->attPosition,
											L"Precompile: Binder \"" +
											setter->binding.ToString() +
											L"\" requires property \"" +
											propertyName.ToString() +
											L"\" to physically appear in type \"" +
											reprTypeInfo.typeName.ToString() +
											L"\"."));
									}
								}
							}
							else
							{
								errors.Add(GuiResourceError({ resolvingResult.resource }, setter->attPosition,
									L"[INTERNAL ERROR] Precompile: The appropriate IGuiInstanceBinder of binding \"-" +
									setter->binding.ToString() +
									L"\" cannot be found."));
							}
						}
						else
						{
							FOREACH(Ptr<GuiValueRepr>, value, setter->values)
							{
								value->Accept(this);
							}
						}
					}

					FOREACH_INDEXER(Ptr<GuiAttSetterRepr::EventValue>, handler, index, repr->eventHandlers.Values())
					{
						if (reprTypeInfo.typeDescriptor)
						{
							GlobalStringKey propertyName = repr->eventHandlers.Keys()[index];
							auto td = reprTypeInfo.typeDescriptor;
							auto eventInfo = td->GetEventByName(propertyName.ToString(), true);

							if (!eventInfo)
							{
								errors.Add(GuiResourceError({ resolvingResult.resource }, handler->attPosition,
									L"[INTERNAL ERROR] Precompile: Event \"" +
									propertyName.ToString() +
									L"\" cannot be found in type \"" +
									reprTypeInfo.typeName.ToString() +
									L"\"."));
							}
							else
							{
								Ptr<WfStatement> statement;

								if (handler->binding == GlobalStringKey::Empty)
								{
									statement = Workflow_InstallEvent(resolvingResult, repr->instanceName, eventInfo, handler->value);
								}
								else
								{
									auto binder = GetInstanceLoaderManager()->GetInstanceEventBinder(handler->binding);
									if (binder)
									{
										statement = binder->GenerateInstallStatement(resolvingResult, repr->instanceName, eventInfo, handler->value, handler->valuePosition, errors);
									}
									else
									{
										errors.Add(GuiResourceError({ resolvingResult.resource }, handler->attPosition,
											L"[INTERNAL ERROR] The appropriate IGuiInstanceEventBinder of binding \"-" +
											handler->binding.ToString() +
											L"\" cannot be found."));
									}
								}

								if (statement)
								{
									statements->statements.Add(statement);
								}
							}
						}
					}

					WORKFLOW_ENVIRONMENT_VARIABLE_REMOVE
				}
			}

			void Visit(GuiConstructorRepr* repr)override
			{
				Visit((GuiAttSetterRepr*)repr);
			}
		};

		void Workflow_GenerateBindings(types::ResolvingResult& resolvingResult, Ptr<WfBlockStatement> statements, GuiResourceError::List& errors)
		{
			WorkflowGenerateBindingVisitor visitor(resolvingResult, statements, errors);
			resolvingResult.context->instance->Accept(&visitor);
		}
	}
}