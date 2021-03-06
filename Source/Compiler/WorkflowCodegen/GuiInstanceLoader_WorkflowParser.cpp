#include "GuiInstanceLoader_WorkflowCodegen.h"
#include "../../Resources/GuiParserManager.h"

namespace vl
{
	namespace presentation
	{
		using namespace reflection::description;
		using namespace workflow;
		using namespace workflow::analyzer;

/***********************************************************************
Parser
***********************************************************************/

		Ptr<workflow::WfExpression> Workflow_ParseExpression(const WString& code, collections::List<Ptr<parsing::ParsingError>>& errors)
		{
			auto parser = GetParserManager()->GetParser<WfExpression>(L"WORKFLOW-EXPRESSION");
			return parser->TypedParse(code, errors);
		}

		Ptr<workflow::WfStatement> Workflow_ParseStatement(const WString& code, collections::List<Ptr<parsing::ParsingError>>& errors)
		{
			auto parser = GetParserManager()->GetParser<WfStatement>(L"WORKFLOW-STATEMENT");
			return parser->TypedParse(code, errors);
		}

/***********************************************************************
Workflow_ModuleToString
***********************************************************************/

		WString Workflow_ModuleToString(Ptr<workflow::WfModule> module)
		{
			stream::MemoryStream stream;
			{
				stream::StreamWriter writer(stream);
				WfPrint(module, L"", writer);
			}
			stream.SeekFromBegin(0);
			stream::StreamReader reader(stream);
			return reader.ReadToEnd();
		}

/***********************************************************************
Converter
***********************************************************************/

		Ptr<workflow::WfExpression> Workflow_ParseTextValue(description::ITypeDescriptor* typeDescriptor, const WString& textValue, collections::List<Ptr<parsing::ParsingError>>& errors)
		{
			if (typeDescriptor == description::GetTypeDescriptor<WString>())
			{
				auto str = MakePtr<WfStringExpression>();
				str->value.value = textValue;
				return str;
			}
			else if (typeDescriptor->GetSerializableType())
			{
				auto str = MakePtr<WfStringExpression>();
				str->value.value = textValue;

				auto type = MakePtr<TypeDescriptorTypeInfo>(typeDescriptor, TypeInfoHint::Normal);

				auto cast = MakePtr<WfTypeCastingExpression>();
				cast->type = GetTypeFromTypeInfo(type.Obj());
				cast->strategy = WfTypeCastingStrategy::Strong;
				cast->expression = str;

				return cast;
			}
			else if (typeDescriptor->GetTypeDescriptorFlags() == TypeDescriptorFlags::Struct)
			{
				vint oldErrorCount = errors.Count();
				auto valueExpr = Workflow_ParseExpression(L"{" + textValue + L"}", errors);
				for (vint i = oldErrorCount; i < errors.Count(); i++)
				{
					auto error = errors[i];
					if (error->codeRange.start.row >= 0)
					{
						errors[i]->codeRange.start.column -= 1; // {
					}
				}

				auto type = MakePtr<TypeDescriptorTypeInfo>(typeDescriptor, TypeInfoHint::Normal);

				auto infer = MakePtr<WfInferExpression>();
				infer->type = GetTypeFromTypeInfo(type.Obj());
				infer->expression = valueExpr;

				return infer;
			}
			else if ((typeDescriptor->GetTypeDescriptorFlags() & TypeDescriptorFlags::EnumType) != TypeDescriptorFlags::Undefined)
			{
				vint oldErrorCount = errors.Count();
				auto valueExpr = Workflow_ParseExpression(L"(" + textValue + L")", errors);
				for (vint i = oldErrorCount; i < errors.Count(); i++)
				{
					auto error = errors[i];
					if (error->codeRange.start.row >= 0)
					{
						errors[i]->codeRange.start.column -= 1; // (
					}
				}

				auto type = MakePtr<TypeDescriptorTypeInfo>(typeDescriptor, TypeInfoHint::Normal);

				auto infer = MakePtr<WfInferExpression>();
				infer->type = GetTypeFromTypeInfo(type.Obj());
				infer->expression = valueExpr;

				return infer;
			}
			else
			{
				CHECK_FAIL(L"vl::presentation::Workflow_ParseTextValue(ITypeDescriptor*, const WString&, GuiResourceError::List&)#This is not a value type.");
			}
		}
	}
}