[@bs.config {jsx: 3}];

open CurriculumEditor__Types;

let markIcon: string = [%raw
  "require('./images/target-complete-mark-icon.svg')"
];
let linkIcon: string = [%raw
  "require('./images/target-complete-link-icon.svg')"
];
let quizIcon: string = [%raw
  "require('./images/target-complete-quiz-icon.svg')"
];

let str = React.string;

type methodOfCompletion =
  | Evaluated
  | VisitLink
  | TakeQuiz
  | MarkAsComplete;

type evaluationCriterion = (int, string, bool);

type prerequisiteTarget = (int, string, bool);

type state = {
  title: string,
  targetGroupId: string,
  role: TargetDetails.role,
  evaluationCriteria: array(string),
  prerequisiteTargets: array(string),
  methodOfCompletion,
  quiz: array(TargetDetails__QuizQuestion.t),
  linkToComplete: string,
  dirty: bool,
  saving: bool,
  loading: bool,
  visibility: TargetDetails.visibility,
  completionInstructions: string,
};

type action =
  | LoadTargetDetails(TargetDetails.t)
  | UpdateTitle(string)
  | UpdatePrerequisiteTargets(prerequisiteTarget)
  | UpdateMethodOfCompletion(methodOfCompletion)
  | UpdateEvaluationCriteria(evaluationCriterion)
  | UpdateLinkToComplete(string)
  | UpdateCompletionInstructions(string)
  | UpdateTargetRole(TargetDetails.role)
  | AddQuizQuestion
  | UpdateQuizQuestion(
      TargetDetails__QuizQuestion.id,
      TargetDetails__QuizQuestion.t,
    )
  | RemoveQuizQuestion(TargetDetails__QuizQuestion.id)
  | UpdateVisibility(TargetDetails.visibility)
  | UpdateSaving;

module TargetDetailsQuery = [%graphql
  {|
    query($targetId: ID!) {
      targetDetails(targetId: $targetId) {
        title
        targetGroupId
        evaluationCriteria
        prerequisiteTargets
        quiz {
          id
          question
          answerOptions {
            id
            answer
            hint
            correctAnswer
          }
        }
        completionInstructions
        visibility
        linkToComplete
        role
      }
  }
|}
];

let loadTargetDetails = (targetId, send) => {
  let response =
    TargetDetailsQuery.make(~targetId, ())
    |> GraphqlQuery.sendQuery(AuthenticityToken.fromHead(), ~notify=true);
  response
  |> Js.Promise.then_(result => {
       let targetDetails = TargetDetails.makeFromJs(result##targetDetails);
       send(LoadTargetDetails(targetDetails));
       Js.Promise.resolve();
     })
  |> ignore;
};

let computeMethodOfCompletion = targetDetails => {
  let hasQuiz = targetDetails |> TargetDetails.quiz |> ArrayUtils.isNotEmpty;
  let hasEvaluationCriteria =
    targetDetails.evaluationCriteria |> ArrayUtils.isNotEmpty;
  let hasLinkToComplete =
    switch (targetDetails.linkToComplete) {
    | Some(_) => true
    | None => false
    };
  switch (hasEvaluationCriteria, hasQuiz, hasLinkToComplete) {
  | (true, _y, _z) => Evaluated
  | (_x, true, _z) => TakeQuiz
  | (_x, _y, true) => VisitLink
  | (false, false, false) => MarkAsComplete
  };
};

let reducer = (state, action) =>
  switch (action) {
  | LoadTargetDetails(targetDetails) =>
    let methodOfCompletion = computeMethodOfCompletion(targetDetails);
    let quiz =
      targetDetails.quiz |> ArrayUtils.isNotEmpty
        ? targetDetails.quiz : [|TargetDetails__QuizQuestion.empty("0")|];
    {
      ...state,
      title: targetDetails.title,
      role: targetDetails.role,
      evaluationCriteria: targetDetails.evaluationCriteria,
      prerequisiteTargets: targetDetails.prerequisiteTargets,
      methodOfCompletion,
      linkToComplete:
        switch (targetDetails.linkToComplete) {
        | Some(link) => link
        | None => ""
        },
      quiz,
      completionInstructions:
        switch (targetDetails.completionInstructions) {
        | Some(instructions) => instructions
        | None => ""
        },
      loading: false,
    };
  | UpdateTitle(title) => {...state, title, dirty: true}
  | UpdatePrerequisiteTargets(prerequisiteTarget) =>
    let (targetId, _title, selected) = prerequisiteTarget;
    let currentPrerequisiteTargets = state.prerequisiteTargets;
    {
      ...state,
      prerequisiteTargets:
        selected
          ? currentPrerequisiteTargets
            |> Js.Array.concat([|targetId |> string_of_int|])
          : currentPrerequisiteTargets
            |> Js.Array.filter(id => id != (targetId |> string_of_int)),
      dirty: true,
    };
  | UpdateMethodOfCompletion(methodOfCompletion) => {
      ...state,
      methodOfCompletion,
      dirty: true,
    }
  | UpdateEvaluationCriteria(evaluationCriterion) =>
    let (evaluationCriterionId, _title, selected) = evaluationCriterion;
    let currentEcIds = state.evaluationCriteria;
    {
      ...state,
      evaluationCriteria:
        selected
          ? currentEcIds
            |> Js.Array.concat([|evaluationCriterionId |> string_of_int|])
          : currentEcIds
            |> Js.Array.filter(id =>
                 id != (evaluationCriterionId |> string_of_int)
               ),
      dirty: true,
    };
  | UpdateLinkToComplete(linkToComplete) => {
      ...state,
      linkToComplete,
      dirty: true,
    }
  | UpdateCompletionInstructions(instruction) => {
      ...state,
      completionInstructions: instruction,
      dirty: true,
    }
  | UpdateTargetRole(role) => {...state, role, dirty: true}
  | AddQuizQuestion =>
    let quiz =
      Array.append(
        state.quiz,
        [|
          TargetDetails__QuizQuestion.empty(
            Js.Date.now() |> Js.Float.toString,
          ),
        |],
      );
    {...state, quiz, dirty: true};
  | UpdateQuizQuestion(id, quizQuestion) =>
    let quiz =
      state.quiz
      |> Array.map(q =>
           TargetDetails__QuizQuestion.id(q) == id ? quizQuestion : q
         );
    {...state, quiz, dirty: true};
  | RemoveQuizQuestion(id) =>
    let quiz =
      state.quiz
      |> Js.Array.filter(q => TargetDetails__QuizQuestion.id(q) != id);
    {...state, quiz, dirty: true};
  | UpdateVisibility(visibility) => {...state, visibility, dirty: true}
  | UpdateSaving => {...state, saving: !state.saving}
  };

let updateTitle = (send, event) => {
  let title = ReactEvent.Form.target(event)##value;
  send(UpdateTitle(title));
};

let eligiblePrerequisiteTargets = (targetId, targets, targetGroups) => {
  let targetGroupId =
    targets
    |> ListUtils.unsafeFind(
         target => targetId == Target.id(target),
         "Unable to find target with ID: " ++ targetId,
       )
    |> Target.targetGroupId;
  let targetGroup =
    targetGroups
    |> Array.of_list
    |> ArrayUtils.unsafeFind(
         tg => TargetGroup.id(tg) == targetGroupId,
         "Cannot find target group with ID: " ++ targetGroupId,
       );
  let levelId = targetGroup |> TargetGroup.levelId;
  let targetGroupsInSameLevel =
    targetGroups
    |> List.filter(tg => TargetGroup.levelId(tg) == levelId)
    |> List.map(tg => TargetGroup.id(tg));
  targets
  |> List.filter(target => !(target |> Target.archived))
  |> List.filter(target =>
       targetGroupsInSameLevel |> List.mem(Target.targetGroupId(target))
     )
  |> List.filter(target => Target.id(target) != targetId);
};

let prerequisiteTargetsForSelector = (targetId, targets, state, targetGroups) => {
  let selectedTargetIds = state.prerequisiteTargets;
  eligiblePrerequisiteTargets(targetId, targets, targetGroups)
  |> List.map(target => {
       let id = target |> Target.id;
       let selected =
         selectedTargetIds
         |> Js.Array.findIndex(selectedTargetId => id == selectedTargetId)
         > (-1);
       (
         target |> Target.id |> int_of_string,
         target |> Target.title,
         selected,
       );
     });
};

let multiSelectPrerequisiteTargetsCB = (send, key, value, selected) => {
  send(UpdatePrerequisiteTargets((key, value, selected)));
};

let multiSelectEvaluationCriteriaCB = (send, key, value, selected) => {
  send(UpdateEvaluationCriteria((key, value, selected)));
};

let prerequisiteTargetEditor = (send, prerequisiteTargetsData) => {
  prerequisiteTargetsData |> ListUtils.isNotEmpty
    ? <div>
        <label
          className="block tracking-wide text-sm font-semibold mb-2"
          htmlFor="prerequisite_targets">
          {"Are there any prerequisite targets?" |> str}
        </label>
        <div id="prerequisite_targets" className="mb-6">
          <School__SelectBox
            noSelectionHeading="No prerequisites selected"
            noSelectionDescription="This target will not have any prerequisites."
            emptyListDescription="There are no other targets available for selection."
            items={
              prerequisiteTargetsData |> School__SelectBox.convertOldItems
            }
            selectCB={
              multiSelectPrerequisiteTargetsCB(send)
              |> School__SelectBox.convertOldCallback
            }
          />
        </div>
      </div>
    : ReasonReact.null;
};

let booleanButtonClasses = bool => {
  let classes = "toggle-button__button";
  classes ++ (bool ? " toggle-button__button--active" : "");
};

let targetRoleClasses = selected => {
  "w-1/2 target-editor__completion-button relative flex border text-sm font-semibold focus:outline-none rounded px-5 py-4 md:px-8 md:py-5 items-center cursor-pointer text-left "
  ++ (
    selected
      ? "target-editor__completion-button--selected bg-gray-200 text-primary-500 border-primary-500"
      : "border-gray-400 hover:bg-gray-200 bg-white"
  );
};

let targetEvaluated = methodOfCompletion =>
  switch (methodOfCompletion) {
  | Evaluated => true
  | VisitLink => false
  | TakeQuiz => false
  | MarkAsComplete => false
  };

let validNumberOfEvaluationCriteria = state =>
  state.evaluationCriteria |> ArrayUtils.isNotEmpty;

let evaluationCriteriaForSelector = (state, evaluationCriteria) => {
  let selectedEcIds = state.evaluationCriteria;
  evaluationCriteria
  |> List.map(ec => {
       let ecId = ec |> EvaluationCriteria.id;
       let selected =
         selectedEcIds
         |> Js.Array.findIndex(selectedEcId => ecId == selectedEcId) > (-1);
       (
         ec |> EvaluationCriteria.id |> int_of_string,
         ec |> EvaluationCriteria.name,
         selected,
       );
     });
};

let evaluationCriteriaEditor = (state, evaluationCriteria, send) => {
  <div id="evaluation_criteria" className="mb-6">
    <label
      className="block tracking-wide text-sm font-semibold mr-6 mb-2"
      htmlFor="evaluation_criteria">
      {"Choose evaluation criteria from your list" |> str}
    </label>
    {validNumberOfEvaluationCriteria(state)
       ? React.null
       : <div className="drawer-right-form__error-msg">
           {"Atleast one has to be selected" |> str}
         </div>}
    <School__SelectBox
      items={
        evaluationCriteriaForSelector(state, evaluationCriteria)
        |> School__SelectBox.convertOldItems
      }
      selectCB={
        multiSelectEvaluationCriteriaCB(send)
        |> School__SelectBox.convertOldCallback
      }
    />
  </div>;
};

let updateLinkToComplete = (send, event) => {
  send(UpdateLinkToComplete(ReactEvent.Form.target(event)##value));
};

let updateCompletionInstructions = (send, event) => {
  send(UpdateCompletionInstructions(ReactEvent.Form.target(event)##value));
};

let updateMethodOfCompletion = (methodOfCompletion, send, event) => {
  ReactEvent.Mouse.preventDefault(event);
  send(UpdateMethodOfCompletion(methodOfCompletion));
};

let updateTargetRole = (role, send, event) => {
  ReactEvent.Mouse.preventDefault(event);
  send(UpdateTargetRole(role));
};

let updateVisibility = (visibility, send, event) => {
  ReactEvent.Mouse.preventDefault(event);
  send(UpdateVisibility(visibility));
};

let linkEditor = (state, send) => {
  <div className="mt-5">
    <label
      className="inline-block tracking-wide text-sm font-semibold"
      htmlFor="link_to_complete">
      {"Link to complete" |> str}
    </label>
    <span> {"*" |> str} </span>
    <input
      className="appearance-none block w-full bg-white border border-gray-400 rounded py-3 px-4 mt-2 leading-tight focus:outline-none focus:bg-white focus:border-gray-500"
      id="link_to_complete"
      type_="text"
      placeholder="Paste link to complete"
      value={state.linkToComplete}
      onChange={updateLinkToComplete(send)}
    />
    {state.linkToComplete |> UrlUtils.isInvalid(false)
       ? <School__InputGroupError message="Enter a valid link" active=true />
       : React.null}
  </div>;
};

let methodOfCompletionButtonClasses = value => {
  let defaultClasses = "target-editor__completion-button relative flex flex-col items-center bg-white border border-gray-400 hover:bg-gray-200 text-sm font-semibold focus:outline-none rounded p-4";
  value
    ? defaultClasses
      ++ " target-editor__completion-button--selected bg-gray-200 text-primary-500 border-primary-500"
    : defaultClasses ++ " opacity-75 text-gray-900";
};

let methodOfCompletionSelector = (state, send) => {
  <div>
    <div className="mb-6">
      <label
        className="block tracking-wide text-sm font-semibold mr-6 mb-3"
        htmlFor="method_of_completion">
        {"How do you want the student to complete the target?" |> str}
      </label>
      <div id="method_of_completion" className="flex -mx-2">
        <div className="w-1/3 px-2">
          <button
            onClick={updateMethodOfCompletion(MarkAsComplete, send)}
            className={methodOfCompletionButtonClasses(
              switch (state.methodOfCompletion) {
              | MarkAsComplete => true
              | _ => false
              },
            )}>
            <div className="mb-1">
              <img className="w-12 h-12" src=markIcon />
            </div>
            {"Simply mark the target as completed." |> str}
          </button>
        </div>
        <div className="w-1/3 px-2">
          <button
            onClick={updateMethodOfCompletion(VisitLink, send)}
            className={methodOfCompletionButtonClasses(
              switch (state.methodOfCompletion) {
              | VisitLink => true
              | _ => false
              },
            )}>
            <div className="mb-1">
              <img className="w-12 h-12" src=linkIcon />
            </div>
            {"Visit a link to complete the target." |> str}
          </button>
        </div>
        <div className="w-1/3 px-2">
          <button
            onClick={updateMethodOfCompletion(TakeQuiz, send)}
            className={methodOfCompletionButtonClasses(
              switch (state.methodOfCompletion) {
              | TakeQuiz => true
              | _ => false
              },
            )}>
            <div className="mb-1">
              <img className="w-12 h-12" src=quizIcon />
            </div>
            {"Take a quiz to complete the target." |> str}
          </button>
        </div>
      </div>
    </div>
  </div>;
};

let isValidQuiz = quiz => {
  quiz
  |> Js.Array.filter(quizQuestion =>
       quizQuestion |> TargetDetails__QuizQuestion.isValidQuizQuestion != true
     )
  |> ArrayUtils.isEmpty;
};

let addQuizQuestion = (send, event) => {
  ReactEvent.Mouse.preventDefault(event);
  send(AddQuizQuestion);
};
let updateQuizQuestionCB = (send, id, quizQuestion) =>
  send(UpdateQuizQuestion(id, quizQuestion));

let removeQuizQuestionCB = (send, id) => send(RemoveQuizQuestion(id));
let questionCanBeRemoved = state => state.quiz |> Array.length > 1;

let quizEditor = (state, send) => {
  <div>
    <h3
      className="block tracking-wide font-semibold mb-2"
      htmlFor="Quiz question 1">
      {"Prepare the quiz now." |> str}
    </h3>
    {isValidQuiz(state.quiz)
       ? ReasonReact.null
       : <School__InputGroupError
           message="All questions must be filled in, and all questions should have at least two answers."
           active=true
         />}
    {state.quiz
     |> Array.mapi((index, quizQuestion) =>
          <CurriculumEditor__TargetQuizQuestion2
            key={quizQuestion |> TargetDetails__QuizQuestion.id}
            questionNumber=index
            quizQuestion
            updateQuizQuestionCB={updateQuizQuestionCB(send)}
            removeQuizQuestionCB={removeQuizQuestionCB(send)}
            questionCanBeRemoved={questionCanBeRemoved(state)}
          />
        )
     |> ReasonReact.array}
    <a
      onClick={addQuizQuestion(send)}
      className="flex items-center bg-gray-200 border border-dashed border-primary-400 hover:bg-white hover:text-primary-500 hover:shadow-md rounded-lg p-3 cursor-pointer my-5">
      <i className="fas fa-plus-circle text-lg" />
      <h5 className="font-semibold ml-2"> {"Add another Question" |> str} </h5>
    </a>
  </div>;
};

let saveDisabled = state => {
  let hasValidTitle = state.title |> String.length > 1;
  let hasValidMethodOfCompletion =
    switch (state.methodOfCompletion) {
    | TakeQuiz => isValidQuiz(state.quiz)
    | MarkAsComplete => true
    | Evaluated => state.evaluationCriteria |> ArrayUtils.isNotEmpty
    | VisitLink => !(state.linkToComplete |> UrlUtils.isInvalid(false))
    };
  !hasValidTitle || !hasValidMethodOfCompletion || !state.dirty || state.saving;
};

module UpdateTargetQuery = [%graphql
  {|
   mutation($id: ID!, $targetGroupId: ID!, $title: String!, $role: String!, $evaluationCriteria: [ID!],$prerequisiteTargets: [ID!], $quiz: [TargetQuizInput!], $completionInstructions: String, $linkToComplete: String, $visibility: String! ) {
     updateTarget(id: $id, targetGroupId: $targetGroupId, title: $title, role: $role, evaluationCriteria: $evaluationCriteria,prerequisiteTargets: $prerequisiteTargets, quiz: $quiz, completionInstructions: $completionInstructions, linkToComplete: $linkToComplete, visibility: $visibility  ) {
        success
       }
     }
   |}
];

let updateTarget = (state, send, event) => {
  ReactEvent.Mouse.preventDefault(event);
  send(UpdateSaving);
  ();
};

[@react.component]
let make = (~targetId, ~targets, ~targetGroups, ~evaluationCriteria) => {
  let (state, send) =
    React.useReducer(
      reducer,
      {
        title: "",
        targetGroupId: "",
        role: TargetDetails.Student,
        evaluationCriteria: [||],
        prerequisiteTargets: [||],
        methodOfCompletion: Evaluated,
        quiz: [||],
        linkToComplete: "",
        dirty: false,
        saving: false,
        loading: true,
        visibility: TargetDetails.Draft,
        completionInstructions: "",
      },
    );
  React.useEffect1(
    () => {
      loadTargetDetails(targetId, send);
      None;
    },
    [|targetId|],
  );

  <div className="max-w-3xl py-6 px-3 mx-auto" id="target-properties">
    {state.loading
       ? SkeletonLoading.multiple(
           ~count=2,
           ~element=SkeletonLoading.contents(),
         )
       : <div className="mt-2">
           <label
             className="inline-block tracking-wide text-sm font-semibold mb-2"
             htmlFor="title">
             {"Title" |> str}
           </label>
           <span> {"*" |> str} </span>
           <div
             className="flex items-center border-b border-gray-400 pb-2 mb-4">
             <input
               className="appearance-none block w-full bg-white text-2xl pr-4 font-semibold text-gray-900 leading-tight hover:border-gray-500 focus:outline-none focus:bg-white focus:border-gray-500"
               id="title"
               type_="text"
               placeholder="Type target title here"
               onChange={updateTitle(send)}
               value={state.title}
             />
           </div>
           <School__InputGroupError
             message="Enter a valid title"
             active={state.title |> String.length < 1}
           />
           {prerequisiteTargetEditor(
              send,
              prerequisiteTargetsForSelector(
                targetId,
                targets,
                state,
                targetGroups,
              ),
            )}
           <div className="flex items-center mb-6">
             <label
               className="block tracking-wide text-sm font-semibold mr-6"
               htmlFor="evaluated">
               {"Will a coach review submissions on this target?" |> str}
             </label>
             <div
               id="evaluated"
               className="flex toggle-button__group flex-shrink-0 rounded-lg overflow-hidden">
               <button
                 onClick={updateMethodOfCompletion(Evaluated, send)}
                 className={booleanButtonClasses(
                   targetEvaluated(state.methodOfCompletion),
                 )}>
                 {"Yes" |> str}
               </button>
               <button
                 onClick={updateMethodOfCompletion(MarkAsComplete, send)}
                 className={booleanButtonClasses(
                   !targetEvaluated(state.methodOfCompletion),
                 )}>
                 {"No" |> str}
               </button>
             </div>
           </div>
           {targetEvaluated(state.methodOfCompletion)
              ? React.null : methodOfCompletionSelector(state, send)}
           {switch (state.methodOfCompletion) {
            | Evaluated =>
              evaluationCriteriaEditor(state, evaluationCriteria, send)
            | MarkAsComplete => React.null
            | TakeQuiz => quizEditor(state, send)
            | VisitLink => linkEditor(state, send)
            }}
           <div className="mt-6">
             <label
               className="inline-block tracking-wide text-sm font-semibold"
               htmlFor="role">
               {"How should teams tackle this target?" |> str}
             </label>
             <HelpIcon
               className="ml-1"
               link="https://docs.pupilfirst.com/#/curriculum_editor?id=setting-the-method-of-completion">
               {"Should students in a team submit work on a target individually, or together?"
                |> str}
             </HelpIcon>
             <div id="role" className="flex mt-4">
               <button
                 onClick={updateTargetRole(TargetDetails.Student, send)}
                 className={
                   "mr-4 "
                   ++ targetRoleClasses(
                        switch (state.role) {
                        | TargetDetails.Student => true
                        | Team => false
                        },
                      )
                 }>
                 <span className="mr-4">
                   <Icon className="if i-users-check-light text-3xl" />
                 </span>
                 <span className="text-sm">
                   {"All students must submit individually." |> str}
                 </span>
               </button>
               <button
                 onClick={updateTargetRole(TargetDetails.Team, send)}
                 className={targetRoleClasses(
                   switch (state.role) {
                   | TargetDetails.Team => true
                   | Student => false
                   },
                 )}>
                 <span className="mr-4">
                   <Icon className="if i-user-check-light text-2xl" />
                 </span>
                 <span className="text-sm">
                   {"Only one student in a team" |> str}
                   <br />
                   {" needs to submit." |> str}
                 </span>
               </button>
             </div>
           </div>
           <div className="mt-6">
             <label
               className="tracking-wide text-sm font-semibold"
               htmlFor="completion-instructions">
               {"Do you have any completion instructions for the student?"
                |> str}
               <span className="ml-1 text-xs font-normal">
                 {"(optional)" |> str}
               </span>
             </label>
             <HelpIcon
               link="https://docs.pupilfirst.com/#/curriculum_editor?id=setting-the-method-of-completion"
               className="ml-1">
               {"Use this to remind the student about something important. These instructions will be displayed close to where students complete the target."
                |> str}
             </HelpIcon>
             <input
               className="appearance-none block w-full bg-white border border-gray-400 rounded py-3 px-4 mt-2 leading-tight focus:outline-none focus:bg-white focus:border-gray-500"
               id="completion-instructions"
               type_="text"
               maxLength=255
               value={state.completionInstructions}
               onChange={updateCompletionInstructions(send)}
             />
           </div>
           <div className="bg-white p-6">
             <div
               className="flex max-w-3xl w-full justify-between items-center px-3 mx-auto">
               <div className="flex items-center flex-shrink-0">
                 <label
                   className="block tracking-wide text-sm font-semibold mr-3"
                   htmlFor="archived">
                   {"Target Visibility" |> str}
                 </label>
                 <div
                   id="visibility"
                   className="flex toggle-button__group flex-shrink-0 rounded-lg overflow-hidden">
                   <button
                     onClick={updateVisibility(Live, send)}
                     className={booleanButtonClasses(
                       switch (state.visibility) {
                       | Live => true
                       | _ => false
                       },
                     )}>
                     {"Live" |> str}
                   </button>
                   <button
                     onClick={updateVisibility(Archived, send)}
                     className={booleanButtonClasses(
                       switch (state.visibility) {
                       | Archived => true
                       | _ => false
                       },
                     )}>
                     {"Archived" |> str}
                   </button>
                   <button
                     onClick={updateVisibility(Draft, send)}
                     className={booleanButtonClasses(
                       switch (state.visibility) {
                       | Draft => true
                       | _ => false
                       },
                     )}>
                     {"Draft" |> str}
                   </button>
                 </div>
               </div>
               <div className="w-auto">
                 <button
                   key="target-actions-step"
                   disabled={saveDisabled(state)}
                   onClick={updateTarget(state, send)}
                   className="btn btn-primary w-full text-white font-bold py-3 px-6 shadow rounded focus:outline-none">
                   {"Update Target" |> str}
                 </button>
               </div>
             </div>
           </div>
         </div>}
  </div>;
};
